#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct __attribute((packed)) {
    uint32_t sequenceNum;
    uint16_t cksum;
    uint8_t flag;
} packetHeader;

int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen);
void printPDU(uint8_t* aPDU, int pduLength);