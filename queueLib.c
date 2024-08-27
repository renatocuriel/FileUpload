#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "queueLib.h"

windowQueue* initWindowQueue(uint32_t windowSize, uint32_t bufferSize) {
    windowQueue* newQueue = malloc(sizeof(windowQueue)); //create new queue ptr 
    if (newQueue == NULL) {
        printf("error allocating memory for queue\n");
        exit(-1);
    }

    newQueue->list = malloc(sizeof(Entry) * windowSize); //allocate memory for array
    if (newQueue->list == NULL) {
        printf("error allocating memory for list of entries\n");
        exit(-1);
    }

    for (int i = 0; i < windowSize; i++) { //allocate memory for each entry's pdu, payload + 7 header bytes
        newQueue->list[i].pdu = malloc(7 + bufferSize);
        if (newQueue->list[i].pdu == NULL) {
            printf("error allocating memory for window entry PDU's\n");
            exit(-1);
        }

        memset(newQueue->list[i].pdu, '\0', 7 + bufferSize); //initalize all pdu's

        newQueue->list[i].payloadSize = 0; //initialize all actual payload sizes
        newQueue->list[i].sequenceNumber = 0; //initalize all sequence numbers
    }

    //update window logic parameters
    newQueue->lower = 0;
    newQueue->current = 0;
    newQueue->upper = windowSize;
    newQueue->windowSize = windowSize;
    newQueue->bufferSize = bufferSize;

    return newQueue; //return queue ptr
}

void addEntry(windowQueue* queue, uint32_t sequenceNumber, uint8_t* pdu, uint32_t pduLen) {
    uint32_t index = sequenceNumber % queue->windowSize; //mod by windowSize to get index

    //update circular queue entry
    queue->list[index].sequenceNumber = sequenceNumber;
    queue->list[index].payloadSize = pduLen - 7;
    memcpy(queue->list[index].pdu, pdu, pduLen);

    //update current
    queue->current++;
}

Entry getEntry(windowQueue* queue, uint32_t sequenceNumber) {
    uint32_t index = sequenceNumber % queue->windowSize; //mod by windowSize to get index

    //get entry from circular queue array
    return queue->list[index];
}

void updateWindow(windowQueue* queue, uint32_t sequenceNumber) {
    queue->lower = sequenceNumber; //update lower
    queue->upper = queue->lower + queue->windowSize; //update upper
}

int windowOpen(windowQueue* queue) {
    return queue->current < queue->upper; //if window open, return 1
}

void printWindow(windowQueue* queue) {
    printf("\n\tWINDOW STATUS: lower: %u current: %u upper: %u\n", queue->lower, queue->current, queue->upper);
    for (int i = 0; i < queue->windowSize; i++) {
        printf("\tindex %d has seq# %u\n", i, queue->list[i].sequenceNumber);
    }
}

void freeQueue(windowQueue* queue) {
    for (int i = 0; i < queue->windowSize; i++) { //free each pdu in each entry
        free(queue->list[i].pdu);
    }

    free(queue->list); //free array of entries
    free(queue); //free queue itself
}