#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "queueLib.h" // Ensure this path is correct
#include "bufferLib.h"
#include "pdu.h"
#include "cpe464.h"
#include "checksum.h"

void printEntry(Entry entry, uint32_t bufferSize) {
    printf("Sequence Number: %u\n", entry.sequenceNumber);
    printf("Entry struct payload size: %u\n", entry.payloadSize);
    printf("Payload: ");
    for (uint32_t i = 0; i < bufferSize; i++) {
        printf("%c", entry.pdu[i+7]);
    }
    printf("\n");
}

// int createPDU(uint8_t* pduBuffer, uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int payloadLen) {
//     uint8_t newPDU[payloadLen + 7]; //create pdu
//     memset(newPDU, 0, payloadLen + 7); //initialize pdu
//     uint32_t networkOrderSequenceNumber = htonl(sequenceNumber); //convert sequence number to network order
//     memcpy(newPDU, &networkOrderSequenceNumber, 4); //put NO seq # into buffer

//     memcpy(newPDU + 6, &flag, 1); //add flag
//     memcpy(newPDU + 7, payload, payloadLen); //add data payload

//     uint16_t checksum = in_cksum((unsigned short *) newPDU, payloadLen + 7); //calculate checksum
//     memcpy(newPDU + 4, &checksum, 2); //add checksum

//     memcpy(pduBuffer, newPDU, payloadLen + 7); //copy buffers

//     return payloadLen + 7;
// }

int main() {
    uint32_t windowSize = 5;
    uint32_t bufferSize = 10; // Example buffer size, matching the initWindowQueue call
    uint8_t pduData[bufferSize]; // Buffer for PDU data

    // Initialize window queue
    windowQueue* queue = initWindowQueue(windowSize, bufferSize);
    if (queue == NULL) {
        printf("Failed to initialize window queue\n");
        return -1;
    }

    // Initialize buffer
    windowBuffer* buffer = initWindowBuffer(windowSize, bufferSize);
    if (buffer == NULL) {
        printf("Failed to initialize window buffer\n");
        return -1;
    }

    // Add entries to the queue with unique PDU data
    uint8_t pduBuffer[bufferSize + 7]; // Temporary buffer for PDU
    for (uint32_t i = 0; i < windowSize; i++) {
        // Generate unique PDU data for each entry
        char tempBuffer[bufferSize];
        sprintf(tempBuffer, "Data%u", i);
        int len = strlen(tempBuffer);
        int pduLen = createPDU(pduBuffer, i, 1, (uint8_t *) tempBuffer, len); //create PDU

        addEntry(queue, i, pduBuffer, pduLen); //add to window queue
        addBufferEntry(buffer, i, pduBuffer, pduLen); //add to buffer
        markValid(buffer, i); //mark entry valid 

        printf("Added entry with strlen %d and new sequence number %u and payload: %s\n", len, i, tempBuffer);

        printPDU(pduBuffer, pduLen);
        memset(pduBuffer, '\0', pduLen);
        // Check window status
        if (windowOpen(queue)) {
            printf("Window is open.\n");
        } else {
            printf("Window is closed.\n");
        }

    }

    // Check and print entries
    for (uint32_t j = 0; j < windowSize; j++) {
        Entry entry = getEntry(queue, j);
        printf("Queue entry %u: ", j);
        printf("payload: %s\n", entry.pdu + 7);
        
        // printf("Sequence Number: %u\n", entry.sequenceNumber);
        // printf("Entry struct payload size: %u\n", entry.payloadSize);
        // printf("Payload: ");
        // for (uint32_t i = 0; i < bufferSize; i++) {
        //     printf("%c", entry.pdu[i+7]);
        // }
        // printf("\n");
    }

    printf("\n");

    uint32_t k = 0;
    uint8_t valid = 0;
    while (valid = checkValid(buffer, k)) {
        bufferEntry bEntry = getBufferEntry(buffer, k);
        printf("Buffer entry %u: ", k);
        printf("payload: %s\n", bEntry.pdu + 7);
        printf("valid flag: %u\n", bEntry.valid);
        markInvalid(buffer, k);
        bEntry = getBufferEntry(buffer,k);
        printf("valid flag: %u\n", bEntry.valid);        
        printf("\n");

        k++;
    }

    for(int l = 0; l < windowSize; l++) {
        printf("Buffer entry %d valid flag: %u\n", l, buffer->list[l].valid);
    }

    uint8_t newpduBuffer[bufferSize + 7];
    for(int a = 0; a < 2; a++) {
        // Generate unique PDU data for each entry
        char newtempBuffer[bufferSize];
        sprintf(newtempBuffer, "NewData%u", a);
        int newlen = strlen(newtempBuffer);
        int newpduLen = createPDU(newpduBuffer, a, 1, (uint8_t *) newtempBuffer, newlen); //create PDU

        addEntry(queue, a, pduBuffer, newpduLen); //add to window queue
        addBufferEntry(buffer, a, newpduBuffer, newpduLen); //add to buffer
        markValid(buffer, a); //mark entry valid 

        printf("Added entry with strlen %d and new sequence number %u and payload: %s\n", newlen, a, newtempBuffer);

        printPDU(newpduBuffer, newpduLen);
        memset(newpduBuffer, '\0', newpduLen);
        // Check window status
        if (windowOpen(queue)) {
            printf("Window is open.\n");
        } else {
            printf("Window is closed.\n");
        }
    }

    //insert hole
    markInvalid(buffer, 2);

    //add another

    // for (uint32_t k = 0; k < windowSize; k++) {
    //     bufferEntry bEntry = getBufferEntry(buffer, k);
    //     printf("Buffer entry %u: ", k);
    //     printf("payload: %s\n", bEntry.pdu + 7);
    //     printf("valid flag: %u\n", bEntry.valid);
    //     markInvalid(buffer, k);
    //     bEntry = getBufferEntry(buffer,k);
    //     printf("valid flag: %u\n", bEntry.valid);

    //     // printf("Sequence Number: %u\n", bEntry.sequenceNumber);
    //     // printf("Entry struct payload size: %u\n", bEntry.payloadSize);
    //     // printf("Valid flag: %u", bEntry.valid);
    //     // printf("Payload: ");
    //     // for (uint32_t i = 0; i < bufferSize; i++) {
    //     //     printf("%c", bEntry.pdu[i+7]);
    //     // }
    //     // printf("\n");
    // }

    // Check window status
    if (windowOpen(queue)) {
        printf("Window is open.\n");
    } else {
        printf("Window is closed.\n");
    }

    // Update window to simulate window sliding
    updateWindow(queue, windowSize);
    printf("Window updated.\n");

    // Check window status
    if (windowOpen(queue)) {
        printf("Window is open.\n");
    } else {
        printf("Window is closed.\n");
    }




    // // Add entries with new sequence numbers to test circular buffer functionality
    // for (uint32_t i = windowSize; i < 2 * windowSize; i++) {
    //     // Generate unique PDU data for each new entry
    //     sprintf(tempBuffer, "12340HEADERRData%u", i);
    //     memcpy(pduData, tempBuffer, strlen(tempBuffer)); // Include null terminator in copy

    //     addEntry(queue, i, pduData);

    //     printf("Added entry with new sequence number %u and PDU: %s\n", i, tempBuffer);

    //     if (windowOpen(queue)) {
    //         printf("Window is open.\n");
    //     } else {
    //         printf("Window is closed.\n");
    //     }

    // }

    // // Check and print entries
    // for (uint32_t i = 0; i < windowSize; i++) {
    //     Entry entry = getEntry(queue, i);
    //     printf("Entry %u: ", i);
    //     printEntry(entry, bufferSize);
    // }

    // // Check window status again
    // if (windowOpen(queue)) {
    //     printf("Window is open.\n");
    // } else {
    //     printf("Window is closed.\n");
    // }

    // Free the queue
    freeQueue(queue);
    printf("Queue freed.\n");

    freeWindowBuffer(buffer);
    printf("Buffer freed.\n");

    return 0;
}
