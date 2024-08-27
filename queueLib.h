#ifndef QUEUELIB_H
#define QUEUELIB_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

typedef struct __attribute__((packed)) {
    uint32_t sequenceNumber; //entry sequence number
    uint32_t payloadSize; //size of payload in pdu
    uint8_t* pdu; //entire pdu
} Entry;

typedef struct __attribute__((packed)) {
    uint32_t lower; //window lower value
    uint32_t current; //window current value
    uint32_t upper; //window upper value
    uint32_t windowSize; //size of window
    uint32_t bufferSize; //max size of data buffer (not including header of size 7)
    Entry* list; //array of entries, managed as a circular queue
} windowQueue;


windowQueue* initWindowQueue(uint32_t windowSize, uint32_t bufferSize);
void addEntry(windowQueue* queue, uint32_t sequenceNumber, uint8_t* pdu, uint32_t pduLen);
Entry getEntry(windowQueue* queue, uint32_t sequenceNumber);
void updateWindow(windowQueue* queue, uint32_t sequenceNumber);
int windowOpen(windowQueue* queue);
void freeQueue(windowQueue* queue);

#endif