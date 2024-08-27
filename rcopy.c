#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "checksum.h"
#include "cpe464.h"
#include "pdu.h"
#include "pollLib.h"
#include "queueLib.h"

#define MAXBUF 1400
#define MINBUF 1

#define DATA 3
#define RESEND 4
#define RR 5
#define SREJ 6
#define TIMEOUT_BREAK 7
#define FILENAME 8
#define GOODFILE 9
#define BADFILE 10
#define EOFPACKET 11
#define EOFACK 12
#define EOFACKACK 13

typedef enum {
	SEND_FILENAME,
	WAIT_ON_FILENAME_ACK,
	SEND_DATA,
	TEARDOWN,
	DONE
} STATE;

void checkArgs(int argc, char * argv[]);
void uploadFile(char* argv[]);
STATE filename(char* hostName, int serverPort, char* toFile, uint32_t windowSize, uint16_t bufferSize, Connection* server, uint32_t* sequenceNum, uint8_t* filenameCount);
STATE filenameResponse(Connection* server, uint8_t* filenameCount, char* toFile, uint32_t windowSize, uint16_t bufferSize);
STATE checkFilenameCount(uint8_t* filenameCount, Connection* server);
STATE sendData(Connection* server, char* fromFile, uint32_t* sequenceNum);
STATE teardown(Connection* server, uint32_t* sequenceNum);
void processReply(Connection* server, uint8_t* pdu, uint32_t* curRR);
void resendLastAcked(Connection* server, uint32_t curRR);
void resendSREJ(Connection* server, uint32_t seqNum);

windowQueue* queue = NULL;

int main (int argc, char *argv[])
 {

	checkArgs(argc, argv);
	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON); //uncomment when all done
	uploadFile(argv);

	return 0;
}

void uploadFile(char* argv[]) {
	setupPollSet(); //start poll set
	Connection server = {0, {0}, 0}; //init connection struct, holds connection info

	uint32_t windowSize = atoi(argv[3]); //window size
	uint16_t bufferSize = atoi(argv[4]); //buffer size (bytes to read/send at a time)
	uint32_t sequenceNum = 0; //sequence num
	uint8_t filenameCount = 0; //filename timeout counter

	STATE currentState = SEND_FILENAME; //start with sending filename
	while (currentState != DONE) {

		switch(currentState) {
			case SEND_FILENAME: 
				currentState = filename(argv[6], atoi(argv[7]), argv[2], windowSize, bufferSize, &server, &sequenceNum, &filenameCount);
			break;

			case WAIT_ON_FILENAME_ACK:
				currentState = filenameResponse(&server, &filenameCount, argv[1], windowSize, bufferSize);
			break;

			case SEND_DATA: 
				currentState = sendData(&server, argv[1], &sequenceNum);
			break;

			case TEARDOWN:
				currentState = teardown(&server, &sequenceNum);
			break;

			case DONE: break;
			
			default: break;
		}

	}

	freeQueue(queue);
}

STATE filename(char* hostName, int serverPort, char* toFile, uint32_t windowSize, uint16_t bufferSize, Connection* server, uint32_t* sequenceNum, uint8_t* filenameCount) {
	if (server->socketNum != 0) { //close connection on retry to prevent race conditions
		close(server->socketNum);
		removeFromPollSet(server->socketNum);
	}

	if (setupUdpClientToServer(server, hostName, serverPort) != 0) { //connect to server
		return DONE;
	}

	addToPollSet(server->socketNum); //add client socket to poll set for timeout purposes

	uint8_t toFileLen = strlen(toFile);
	uint8_t flag = FILENAME;
	uint8_t payloadLen = toFileLen + 4 + 2 + 1;

	uint8_t payload[payloadLen]; //create payload buf
	memcpy(payload, &windowSize, 4);
	memcpy(payload + 4, &bufferSize, 2);
	memcpy(payload + 6, toFile, toFileLen);
	payload[payloadLen] = '\0';

	uint8_t pdu[7 + payloadLen]; //create pdu buf
	int pduLen = createPDU(pdu, *sequenceNum, flag, payload, payloadLen);

	safeSendto(server->socketNum, pdu, pduLen, 0, (struct sockaddr *) &(server->remote), (int) server->len);
	(*sequenceNum)++; //increment sequence num
	(*filenameCount)++; //increment times sent
	return WAIT_ON_FILENAME_ACK; //Clean up, no server response

}

STATE filenameResponse(Connection* server, uint8_t* filenameCount, char* toFile, uint32_t windowSize, uint16_t bufferSize) {
	int sock = pollCall(1000); //wait 1 second for response

	if (sock == server->socketNum) {
		uint8_t recvPDU[1400 + 7] = {0};
		int recvPDUlen = safeRecvfrom(server->socketNum, recvPDU, 1400 + 7, 0, (struct sockaddr *) &(server->remote), (int *) &server->len);

		if (in_cksum((uint16_t *) recvPDU, recvPDUlen) != 0) { //bad checksum
			(*filenameCount)--; //undo counter increment from sendFilename
			return checkFilenameCount(filenameCount, server);
		}
		else { //good checksum
			packetHeader* newPack = (packetHeader *) recvPDU;
			if (newPack->flag == GOODFILE) {// init queue, send data
				queue = initWindowQueue(windowSize, bufferSize);
				return SEND_DATA;
			}
			else if (newPack->flag == BADFILE) {
				printf("Error on open of output file on server: %s.\n", toFile);
				removeFromPollSet(server->socketNum);
				close(server->socketNum);
				return DONE;
			}
			else {
				(*filenameCount)--; //undo counter increment from sendFilename (should never get here)
				return checkFilenameCount(filenameCount, server);
			}
		}
	}
	else if (sock == -1) { //timeout
		return checkFilenameCount(filenameCount, server);
	}

	removeFromPollSet(server->socketNum);
	close(server->socketNum);
	return DONE;
}

STATE sendData(Connection* server, char* fromFile, uint32_t* sequenceNum) {
	int32_t fromFileFD = 0;
	if ((fromFileFD = open(fromFile, O_RDONLY)) < 0) {
		perror("open() call");
		return DONE;
	}

	updateWindow(queue, *sequenceNum); //update window logic to take care of possible filename packets
	queue->current = *sequenceNum; //ensure current begins in correct place

	int EOFflag = 0; //to break out of main EOF loop
	uint32_t curRR = 0; //to know what my actual last acked packet was for EOF
	uint32_t lastDataSeqNum = 0; //to know what RR to expect before sending EOF packet
	uint8_t dataBuf[queue->bufferSize]; //data buffer
	memset(dataBuf, 0, queue->bufferSize); //init buffer
	uint8_t pduBuf[queue->bufferSize + 7]; //pdu buffer
	memset(pduBuf, 0, queue->bufferSize + 7); //init pdu buffer

	while (!EOFflag) {
		while (windowOpen(queue)) { //WINDOW OPEN
			int bytesRead = read(fromFileFD, dataBuf, queue->bufferSize); //read from disk

			if (bytesRead == 0) { //EOF condition
				EOFflag = 1;
				lastDataSeqNum = *sequenceNum;
				break; // exit loop
			}

			//send packet
			int pduLen = createPDU(pduBuf, *sequenceNum, DATA, dataBuf, bytesRead);
			safeSendto(server->socketNum, pduBuf, pduLen, 0, (struct sockaddr *) &(server->remote), (int) server->len);

			//store packet
			addEntry(queue, *sequenceNum, pduBuf, pduLen);
			(*sequenceNum)++; //inc seq num

			int sock = 0;
			while ((sock = pollCall(0)) > 0) { //non-blocking for processing RR/SREJ
				safeRecvfrom(server->socketNum, pduBuf, 11, 0, (struct sockaddr *) &(server->remote), (int *) &(server->len));

				if (in_cksum((uint16_t *) pduBuf, 11) == 0) { //cksum
					processReply(server, pduBuf, &curRR);
				}
			}
		}

		int innerSendCount = 0; //create count for sending timeout packet
		while(!windowOpen(queue)) { //WINDOW CLOSED
			int sock = 0;

			if ((sock = pollCall(1000)) > 0) { //process RR/SREJ with 1 second timeout
				safeRecvfrom(server->socketNum, pduBuf, 11, 0, (struct sockaddr *) &(server->remote), (int *) &(server->len)); 

				if (in_cksum((uint16_t *) pduBuf, 11) == 0) { //cksum
					processReply(server, pduBuf, &curRR);
				}
			}
			else { //TIMEOUT
				if (innerSendCount > 9) {
					printf("Lost contact with server, timed out while sending data.\n");
					removeFromPollSet(server->socketNum);
					close(server->socketNum);
					return DONE;
				}

				resendLastAcked(server, curRR);				
				innerSendCount++; //increment send ctr
			}
		}
	}

	//Handle EOF, Catch up
	int outerSendCount = 0; //create count for catch up timeout packet
	while(curRR != (lastDataSeqNum)) { //while not caught up
		int sock = 0;
		if ((sock = pollCall(1000)) > 0) { //CATCH UP
			safeRecvfrom(server->socketNum, pduBuf, 11, 0, (struct sockaddr *) &(server->remote), (int *) &(server->len));

			if (in_cksum((uint16_t *) pduBuf, 11) == 0) { //cksum
				processReply(server, pduBuf, &curRR);
			}
		}
		else { //TIMEOUT
			if (outerSendCount > 9) {
				printf("Lost contact with server, timed out while catching up on last %u packets.\n", queue->windowSize);
				removeFromPollSet(server->socketNum);
				close(server->socketNum);
				return DONE;
			}

			resendLastAcked(server, curRR);
			outerSendCount++;
		}
	}

	return TEARDOWN;
}

STATE teardown(Connection* server, uint32_t* sequenceNum) {
	uint8_t dummyPayload[2] = {0};
	uint8_t pduBuf[7 + 1] = {0};

	//send up to 10 times while waiting on an EOF acknowledgement, send EOF ack ack if received
	int eofCount = 0;
	while (eofCount < 10) {

		int pduLen = createPDU(pduBuf, *sequenceNum, EOFPACKET, dummyPayload, 1); //create EOF packet
		safeSendto(server->socketNum, pduBuf, pduLen, 0, (struct sockaddr *) &(server->remote), (int) server->len);
		(*sequenceNum)++;

		int socket = 0;
		if ((socket = pollCall(1000)) > 0) { //good poll, receive ack, close rcopy gracefully
			int bytesRecv = safeRecvfrom(server->socketNum, pduBuf, 8, 0, (struct sockaddr *) &(server->remote), (int *) &(server->len));

			if (in_cksum((uint16_t *) pduBuf, bytesRecv) == 0) {
				packetHeader* eofACK = (packetHeader *) pduBuf;

				if (eofACK->flag == EOFACK) { //PROGRAM FINISH
					int lastPduLen = createPDU(pduBuf, *sequenceNum, EOFACKACK, dummyPayload, 1);
					safeSendto(server->socketNum, pduBuf, lastPduLen, 0, (struct sockaddr *) &(server->remote), (int) server->len); //send final ACK
					removeFromPollSet(server->socketNum);
					close(server->socketNum);
					return DONE;
				}
			}
		}
		else {//time out, send again
			eofCount++;
		}

	}

	printf("Lost contact with server, timed out on EOF exchange.\n");
	removeFromPollSet(server->socketNum);
	close(server->socketNum);
	return DONE;
}

void processReply(Connection* server, uint8_t* pdu, uint32_t* curRR) {
	packetHeader* curHeader = (packetHeader *) pdu;
	uint32_t recvSeqNum = 0;
	memcpy(&recvSeqNum, pdu + 7, 4); //grab sequence number from packet, in network order
	if (curHeader->flag == RR) { //update current RR, update window
		*curRR = ntohl(recvSeqNum);
		updateWindow(queue, *curRR);
	}
	else if (curHeader->flag == SREJ) { //resend SREJ'd seq #
		resendSREJ(server, ntohl(recvSeqNum)); 
	}
}

void resendLastAcked(Connection* server, uint32_t curRR) { //get entry, create PDU with TIMEOUT flag, send
	Entry lastAcked = getEntry(queue, queue->lower); 
	uint8_t newPDU[lastAcked.payloadSize + 7];
	memset(newPDU, 0, lastAcked.payloadSize + 7);
	int newLen = createPDU(newPDU, queue->lower, TIMEOUT_BREAK, lastAcked.pdu + 7, lastAcked.payloadSize);
	safeSendto(server->socketNum, newPDU, newLen, 0, (struct sockaddr *) &(server->remote), (int) server->len);
}

void resendSREJ(Connection* server, uint32_t seqNum) { //get entry, create PDU with RESEND flag, send
	Entry rejected = getEntry(queue, seqNum);
	uint8_t newPDU[rejected.payloadSize + 7];
	memset(newPDU, 0, rejected.payloadSize + 7);
	int newLen = createPDU(newPDU, rejected.sequenceNumber, RESEND, rejected.pdu + 7, rejected.payloadSize);
	safeSendto(server->socketNum, newPDU, newLen, 0, (struct sockaddr *) &(server->remote), (int) server->len);
}

void checkArgs(int argc, char * argv[])
{
    /* check command line arguments  */
	if (argc != 8)
	{
		printf("usage: rcopy from-filename to-filename window-size buffer-size error-percent remote-machine remote-port\n");
		exit(1);
	}
	
	char* fromFile = NULL;
	char* toFile = NULL;
	double windowSize = 0;
	int bufferSize = 0;

	fromFile = argv[1]; //check from-file
	toFile = argv[2]; //check to-file len
	if (access(fromFile, F_OK) != 0) {
		printf("Error: file %s not found.\n", fromFile);
		exit(1);
	}

	if ((strlen(fromFile) > 100) || (strlen(toFile) > 100)) {
		printf("Error: from-file/to-file must be 100 characters or less.\n");
		exit(1);
	}

	windowSize = atof(argv[3]); //check window size
	bufferSize = atoi(argv[4]); //check buffer size
	if ((windowSize > pow(2, 30))) {
		printf("Error: window size must be less than 2^30\n");
		exit(1);
	}

	if ((bufferSize < 1) || (bufferSize > 1400)) {
		printf("Error: buffer size must be between 1 and 1400\n");
		exit(1);
	}

}

STATE checkFilenameCount(uint8_t* filenameCount, Connection* server) {
	if (*filenameCount > 9) { //no more tries
		printf("Server is down.\n");
		removeFromPollSet(server->socketNum);
		close(server->socketNum);
		return DONE;
	}
	else {
		return SEND_FILENAME;
	}
}
