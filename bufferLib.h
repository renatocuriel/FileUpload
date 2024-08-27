#ifndef BUFFERLIB_H
#define BUFFERLIB_H

#include <stdio.h>
#include <stdint.h>
#include <stdint.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    uint8_t valid; //flag to determine if entry is valid (buffered)
    uint32_t sequenceNumber; //entry's sequence number
    uint32_t payloadSize; //size of payload (pdu size - 7)
    uint8_t *pdu; //pdu (including 7 byte header)
} bufferEntry;

typedef struct __attribute__((packed)) {
    uint32_t windowSize; //window size
    uint32_t bufferSize; //buffer size (size of pdu - 7)
    bufferEntry *list; //array of buffer entries
} windowBuffer;

windowBuffer* initWindowBuffer(uint32_t windowSize, uint32_t bufferSize);
void addBufferEntry(windowBuffer* windowBuf, uint32_t sequenceNumber, uint8_t* pdu, uint32_t pduLen);
bufferEntry getBufferEntry(windowBuffer* windowBuf, uint32_t sequenceNumber);
void markInvalid(windowBuffer* windowBuf, uint32_t sequenceNumber);
void markValid(windowBuffer* windowBuf, uint32_t sequenceNumber);
uint8_t checkValid(windowBuffer* windowBuf, uint32_t sequenceNumber);
void freeWindowBuffer(windowBuffer* windowBuf);

#endif