#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdint.h>
#include <string.h>
#include "bufferLib.h"

windowBuffer* initWindowBuffer(uint32_t windowSize, uint32_t bufferSize) {
    windowBuffer* newBuf = malloc(sizeof(windowBuffer)); //allocate memory for window buffer struct
    if (newBuf == NULL) {
        printf("error allocating memory for buffer\n");
        exit(-1);
    }

    newBuf->list = malloc(sizeof(bufferEntry) * windowSize); //allocate memory for buffer entry list
    if (newBuf->list == NULL) {
        printf("error allocating memory for buffer entry list\n");
        exit(-1);
    }

    for (int i = 0; i < windowSize; i++) {
        newBuf->list[i].pdu = malloc(7 + bufferSize); //allocate memory for each pdu in each entry
        if (newBuf->list[i].pdu == NULL) {
            printf("error allocating memory for buffer entry list\n");
            exit(-1);
        }

        memset(newBuf->list[i].pdu, '\0', bufferSize + 7); //set all pdu's to be full of null

        newBuf->list[i].sequenceNumber = 0; //initialize all sequence numbers to 0
        newBuf->list[i].payloadSize = 0; //initalize all actual payload sizes to 0
        newBuf->list[i].valid = 0; //initialize all valid flags to 0
    }

    //update window size and buffer size
    newBuf->bufferSize = bufferSize;
    newBuf->windowSize = windowSize;

    return newBuf; //return buffer ptr
}

void addBufferEntry(windowBuffer* windowBuf, uint32_t sequenceNumber, uint8_t* pdu, uint32_t pduLen) {
    uint32_t index = sequenceNumber % windowBuf->windowSize; //get index by modding

    //update entry, mark valid/buffered
    windowBuf->list[index].payloadSize = pduLen - 7;
    windowBuf->list[index].sequenceNumber = sequenceNumber;
    windowBuf->list[index].valid = 1;
    memcpy(windowBuf->list[index].pdu, pdu, pduLen);
}

bufferEntry getBufferEntry(windowBuffer* windowBuf, uint32_t sequenceNumber) {
    uint32_t index = sequenceNumber % windowBuf->windowSize; //get index by modding

    //return bufferEntry
    return windowBuf->list[index];
}

void markInvalid(windowBuffer* windowBuf, uint32_t sequenceNumber) {
    uint32_t index = sequenceNumber % windowBuf->windowSize; //mod by windowSize for circular functionality

    windowBuf->list[index].valid = 0; //make entry invalid
}

void markValid(windowBuffer* windowBuf, uint32_t sequenceNumber) {
    uint32_t index = sequenceNumber % windowBuf->windowSize; //mod by windowSize for circular functionality

    windowBuf->list[index].valid = 1; //make entry valid
}

uint8_t checkValid(windowBuffer* windowBuf, uint32_t sequenceNumber) {
    uint32_t index = sequenceNumber % windowBuf->windowSize; //mod by windowSize for circular functionality

    return windowBuf->list[index].valid;
}

void freeWindowBuffer(windowBuffer* windowBuf) {
    for (int i = 0; i < windowBuf->windowSize; i++) { //free each pdu in each entry
        free(windowBuf->list[i].pdu);
    }

    free(windowBuf->list); //free array of entries
    free(windowBuf); //free queue itself
}