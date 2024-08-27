#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "gethostbyname.h"
#include "networks.h"
#include "safeUtil.h"
#include "checksum.h"
#include "cpe464.h"
#include "pdu.h"
#include "pollLib.h"
#include "bufferLib.h"

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
	SEND_FILENAME_ACK,
	IN_ORDER,
	BUFFER,
	FLUSH,
	TEARDOWN,
	DONE
} STATE;

void handleZombies(int sig);
void startServer(int portNumber);
void processClient(Connection* client, int mainServerSocket, uint8_t* pdu, int pduLen);
STATE sendFilenameAck(Connection* client, int newServerSocket, uint8_t* pdu, int pduLen, uint32_t* sequenceNum, char* toFileName, int32_t* outputFD);
STATE inOrderData(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum);
STATE bufferData(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum);
STATE flushBuffer(Connection* client, int newServerSocket, char* toFIleName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum);
STATE teardown(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* sequenceNum);
void sendRR(Connection* client, int newServerSocket, uint32_t* expected, uint32_t* sequenceNum);
void sendSREJ(Connection* client, int newServerSocket, uint32_t* expected, uint32_t *sequenceNum);
int checkArgs(int argc, char *argv[]);

windowBuffer* circularBuffer = NULL;
double errorRate = 0;

int main ( int argc, char *argv[]  )
{ 			
	int portNumber = 0;

	portNumber = checkArgs(argc, argv);
	
	char* ER = argv[1];
	errorRate = atof(ER);
	
	sendtoErr_init(errorRate, DROP_ON, FLIP_ON, DEBUG_OFF, RSEED_ON);
	handleZombies(SIGCHLD);

	startServer(portNumber);
	
	return 0;
}

void startServer(int portNumber) {
	Connection mainServer = {0 , {0}, 0};
	Connection client = {0, {0}, 0};
	client.len = sizeof(struct sockaddr_in6);

	if (udpServerSetup(&mainServer, portNumber) < 0) { //set up server
		printf("Server set up error\n");
		exit(-1);
	}

	//get new client and fork
	pid_t pid = 0;
	while (1) {
		uint8_t recvPDU[MAXBUF + 7] = {0};
		int pduLen = safeRecvfrom(mainServer.socketNum, recvPDU, MAXBUF + 7, 0, (struct sockaddr *) &(client.remote), (int *) &(client.len));
		if (in_cksum((uint16_t *) recvPDU, pduLen) == 0) { //only fork for good checksum
			if ((pid = fork()) < 0) { //fork error
				perror("fork");
				exit(-1);
			}
			
			if (pid == 0) { //child process!
				sendtoErr_init(errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
				processClient(&client, mainServer.socketNum, recvPDU, pduLen); //process file

				exit(0);
			}
		}
	}
}

void processClient(Connection* client, int mainServerSocket, uint8_t* pdu, int pduLen)
{
	close(mainServerSocket); //close main server socket
	int newServerSocket = 0;
	if ((newServerSocket = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) { //create new UDP server socket
		perror("socket call");
		exit(-1);
	}

	setupPollSet(); //create poll set for time out functionality (10s on server side)
	addToPollSet(newServerSocket);

	packetHeader* filePacket = (packetHeader *) pdu;

	int32_t outputFD = 0; //file descriptor for output file
	char toFileName[101] = {'\0'}; //init filename max buf +1 for null
	uint32_t sequenceNum = 1; //init sequence num (0 packet was for FilenameAck)
	uint32_t expected = ntohl(filePacket->sequenceNum) + 1; //init currently expected sequence num
	uint32_t highest = 0; //highest variable for flushing
	STATE currentState = SEND_FILENAME_ACK; //start at filename ack state

	while (currentState != DONE) {
		switch(currentState) {
			case SEND_FILENAME_ACK:
				currentState = sendFilenameAck(client, newServerSocket, pdu, pduLen, &sequenceNum, toFileName, &outputFD);
			break;

			case IN_ORDER:
				currentState = inOrderData(client, newServerSocket, toFileName, &outputFD, &expected, &highest, &sequenceNum);
			break;

			case BUFFER: 
				currentState = bufferData(client, newServerSocket, toFileName, &outputFD, &expected, &highest, &sequenceNum);
			break;

			case FLUSH: 
				currentState = flushBuffer(client, newServerSocket, toFileName, &outputFD, &expected, &highest, &sequenceNum);
			break;

			case TEARDOWN: 
				currentState = teardown(client, newServerSocket, toFileName, &outputFD, &sequenceNum);
			break;

			case DONE: break;
			
			default: break;
		}
	}

	//clean up
	removeFromPollSet(newServerSocket);
	close(newServerSocket);
}

STATE sendFilenameAck(Connection* client, int newServerSocket, uint8_t* pdu, int pduLen, uint32_t* sequenceNum, char* toFileName, int32_t* outputFD) {

	packetHeader *filePacket = (packetHeader *) pdu;

	if(filePacket->flag == FILENAME) { //parse filename pdu
		uint32_t windowSize = 0;
		uint16_t bufferSize = 0;

		memcpy(&windowSize, pdu + 7, 4); //get window size
		memcpy(&bufferSize, pdu + 11, 2); //get buffer size
		memcpy(toFileName, pdu + 13, pduLen - 7 - 4 - 2 - 1); //get file name which was sent over as a nullterm str

		int flag = 0;
		STATE nextState = IN_ORDER;

		if((*outputFD = open(toFileName,  O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) { //error opening toFile
			flag = BADFILE;
			perror("File open error");
			nextState = DONE;
		}
		else { //good open, initialize windowBuffer
			printf("File %s opened!\n", toFileName);
			flag = GOODFILE;
			circularBuffer = initWindowBuffer(windowSize, bufferSize);
		}

		//send filename ack
		uint8_t dummyPayload[2] = {'\0'};
		uint8_t responsePDU[8] = {0};
		int responseLen = createPDU(responsePDU, *sequenceNum, flag, dummyPayload, 1);
		safeSendto(newServerSocket, responsePDU, responseLen, 0, (struct sockaddr *) &(client->remote), client->len);
		(*sequenceNum)++;

		return nextState;
	}

	//if not a filename packet, exit child
	return DONE;
}

STATE inOrderData(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum) {
	int sock = 0;

	if (!((sock = pollCall(10000)) > 0)) { //time out, close and clean up
		printf("Lost connection with client.\n");
		return DONE;
	}

	uint8_t pduBuf[circularBuffer->bufferSize + 7]; //create pdu
	memset(pduBuf, 0, circularBuffer->bufferSize + 7);
	int bytesRecv = safeRecvfrom(newServerSocket, pduBuf, circularBuffer->bufferSize + 7, 0, (struct sockaddr *) &(client->remote), (int *) &(client->len)); //recv data
	packetHeader* curHeader = (packetHeader *) pduBuf;
	uint32_t curSeqNum = ntohl(curHeader->sequenceNum);	//received sequence #, host order
	int goodChecksum = (in_cksum((uint16_t *) pduBuf, bytesRecv) == 0); //high if good checksum
	STATE nextState = IN_ORDER; //init next state var

	if (curHeader->flag == EOFPACKET) { //data all caught up, move to teardown
		return TEARDOWN;
	}

	if (goodChecksum && (curSeqNum < *expected)) { //good checksum, recv is less than expected
		sendRR(client, newServerSocket, expected, sequenceNum);
	}
	else if (goodChecksum && (curSeqNum == *expected)) { // good checksum, recv = exp
		write(*outputFD, pduBuf + 7, bytesRecv - 7);
		*highest = *expected;
		(*expected)++;
		sendRR(client, newServerSocket, expected, sequenceNum);
	}
	else if (goodChecksum && (curSeqNum > *expected)) { // good checksum, recv > exp
		sendSREJ(client, newServerSocket, expected, sequenceNum);
		addBufferEntry(circularBuffer, curSeqNum, pduBuf, bytesRecv);
		*highest = curSeqNum;
		nextState = BUFFER;
	}

	return nextState;
}

STATE bufferData(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum) {
	int sock = 0;

	if (!((sock = pollCall(10000)) > 0)) { //time out, close and clean up
		printf("Lost connection with client.\n");
		return DONE;
	}

	uint8_t pduBuf[circularBuffer->bufferSize + 7]; //create pdu
	memset(pduBuf, 0, circularBuffer->bufferSize + 7);
	int bytesRecv = safeRecvfrom(newServerSocket, pduBuf, circularBuffer->bufferSize + 7, 0, (struct sockaddr *) &(client->remote), (int *) &(client->len)); //recv data
	packetHeader* curHeader = (packetHeader *) pduBuf;
	uint32_t curSeqNum = ntohl(curHeader->sequenceNum);	//received sequence #, host order
	int goodChecksum = (in_cksum((uint16_t *) pduBuf, bytesRecv) == 0); //high if good checksum
	STATE nextState = BUFFER; //init next state var

	if (goodChecksum && (curSeqNum < *expected)) { //good checksum, recv is less than expected
		sendRR(client, newServerSocket, expected, sequenceNum);
	}
	else if (goodChecksum && (curSeqNum == *expected)) { // good checksum, recv = exp
		write(*outputFD, pduBuf + 7, bytesRecv - 7);
		(*expected)++;
		nextState = FLUSH;
	}
	else if (goodChecksum && (curSeqNum > *expected)) { // good checksum, recv > exp
		addBufferEntry(circularBuffer, curSeqNum, pduBuf, bytesRecv);
		*highest = curSeqNum;
	}

	return nextState;
}

STATE flushBuffer(Connection* client, int newServerSocket, char* toFIleName, int32_t* outputFD, uint32_t* expected, uint32_t* highest, uint32_t* sequenceNum) {
	STATE nextState = BUFFER;
	
	while ((checkValid(circularBuffer, *expected)) && (*expected < *highest)) { //write to disk as long as entry is valid and haven't caught up
		bufferEntry curEntry = getBufferEntry(circularBuffer, *expected);
		write(*outputFD, curEntry.pdu + 7, curEntry.payloadSize);
		markInvalid(circularBuffer, *expected);
		(*expected)++;
	}

	if (*expected == *highest) { //all caught up, back to in order
		bufferEntry lastEntry = getBufferEntry(circularBuffer, *expected);
		write(*outputFD, lastEntry.pdu + 7, lastEntry.payloadSize);

		markInvalid(circularBuffer, *expected);
		(*expected)++;
		sendRR(client, newServerSocket, expected, sequenceNum);
		
		nextState = IN_ORDER;
	}
	else { //not caught up, continue buffering
		sendSREJ(client, newServerSocket, expected, sequenceNum);
		sendRR(client, newServerSocket, expected, sequenceNum);
	}

	return nextState;
}

STATE teardown(Connection* client, int newServerSocket, char* toFileName, int32_t* outputFD, uint32_t* sequenceNum) {
	uint8_t dummyPayload[2] = {0};
	uint8_t eofAckPDU[8] = {0};
	int eofAckLen = createPDU(eofAckPDU, *sequenceNum, EOFACK, dummyPayload, 1);

	safeSendto(newServerSocket, eofAckPDU, eofAckLen, 0, (struct sockaddr *) &(client->remote), (int) client->len);
	(*sequenceNum)++;

	STATE nextState = TEARDOWN;
	int socket = 0;
	if ((socket = pollCall(10000)) < 0) {
		printf("Lost connection with client on EOF\n");
		close(*outputFD);
		freeWindowBuffer(circularBuffer);
		return DONE;
	}

	safeRecvfrom(newServerSocket, eofAckPDU, 8, 0, (struct sockaddr *) &(client->remote), (int *) &(client->len));
	packetHeader* eofeofPacket = (packetHeader *) eofAckPDU;
	if (eofeofPacket->flag == EOFACKACK) { //graceful termination
		close(*outputFD);
		freeWindowBuffer(circularBuffer);
		nextState = DONE;
	}
	
	return nextState;
}

void sendRR(Connection* client, int newServerSocket, uint32_t* expected, uint32_t* sequenceNum) {
	uint32_t networkExpected = htonl(*expected);
	uint8_t payload[4] = {0};
	memcpy(payload, &networkExpected, 4);
	uint8_t rrPDU[11] = {0};
	int rrPDUsize = createPDU(rrPDU, *sequenceNum, RR, payload, 4);
	safeSendto(newServerSocket, rrPDU, rrPDUsize, 0, (struct sockaddr *) &(client->remote), (int) client->len);
	(*sequenceNum)++;
}

void sendSREJ(Connection* client, int newServerSocket, uint32_t* expected, uint32_t *sequenceNum) {
	uint32_t networkExpected = htonl(*expected);
	uint8_t payload[4] = {0};
	memcpy(payload, &networkExpected, 4);
	uint8_t srejPDU[11] = {0};
	int srejPDUsize = createPDU(srejPDU, *sequenceNum, SREJ, payload, 4);
	safeSendto(newServerSocket, srejPDU, srejPDUsize, 0, (struct sockaddr *) &(client->remote), (int) client->len);
	(*sequenceNum)++;	
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if ((argc > 3) || (argc < 2))
	{
		fprintf(stderr, "usage: server err-rate [optional port number]\n");
		exit(-1);
	}
	
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	
	return portNumber;
}

void handleZombies(int sig) {
	int stat = 0;
	while (waitpid(-1, &stat, WNOHANG) > 0)
	{}
}
