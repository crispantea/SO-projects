#ifndef QUEUE_H
#define QUEUE_H

#include "common/constants.h"

typedef struct Node {
    char requestPipe[MAX_PIPE_NAME];
    char responsePipe[MAX_PIPE_NAME];
    struct Node* next;
} Node;

typedef struct Queue {
    Node* head;
    Node* tail;
} Queue;

Queue* initializeQueue();
int isEmptyQueue(Queue* q);
void addToQueue(Queue* q, const char* bufferRequest, const char* bufferResponse);
void removeHeadQueue(Queue* q);
Node* getHeadQueue(Queue* q);
void freeQueue(Queue* q);


#endif
