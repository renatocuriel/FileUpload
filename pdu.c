#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include "cpe464.h"
#include "pdu.h"

//int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen);
//void printPDU(uint8_t* aPDU, int pduLength);

int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen) {
    uint8_t newPDU[payloadLen + 7]; //create pdu
    memset(newPDU, 0, payloadLen + 7); //initialize pdu
    uint32_t networkOrderSequenceNumber = htonl(sequenceNumber); //convert sequence number to network order
    memcpy(newPDU, &networkOrderSequenceNumber, 4); //put NO seq # into buffer

    memcpy(newPDU + 6, &flag, 1); //add flag
    memcpy(newPDU + 7, payload, payloadLen); //add data payload

    uint16_t checksum = in_cksum((unsigned short *) newPDU, payloadLen + 7); //calculate checksum
    memcpy(newPDU + 4, &checksum, 2); //add checksum

    memcpy(pduBuffer, newPDU, payloadLen + 7); //copy buffers

    return payloadLen + 7;
}



void printPDU(uint8_t* aPDU, int pduLength) {
    uint16_t checksum = in_cksum((unsigned short *) aPDU, pduLength); //calculate checksum
    if (checksum != 0) {
        printf("\n\t---------------Corrupted PDU---------------\n");
    }

    //print rest of data
    uint32_t networkOrderSequenceNumber = 0;
    memcpy(&networkOrderSequenceNumber, aPDU, 4);
    uint32_t hostOrderSequenceNumber = ntohl(networkOrderSequenceNumber);
    uint16_t checksumToPrint = 0;
    memcpy(&checksumToPrint, aPDU + 4, 2);
    uint8_t flag = 0;
    memcpy(&flag, aPDU + 6, 1);
    uint8_t data[pduLength - 7];
    memset(data, 0, pduLength - 7);
    memcpy(data, aPDU + 7, pduLength - 7);

    char printData[pduLength - 7 + 1];
    memcpy(printData, data, pduLength - 7);
    printData[pduLength - 7] = '\0';

    printf("\tSequence Number: %d\n\tChecksum: 0x%x\n\tFlag: %d\n\tData Length: %d\n\tData: %s\n",
        hostOrderSequenceNumber, checksumToPrint, flag, pduLength - 7, printData);

    return;
}