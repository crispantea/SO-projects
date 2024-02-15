#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


Queue* initializeQueue() {
    Queue* q = (Queue*) malloc(sizeof(Queue));
    if (!q) {
        exit(EXIT_FAILURE);
        return NULL;
    }
    q->head = q->tail = NULL;
    return q;
}

int isEmptyQueue(Queue* q) {
    return q->head == NULL;
}

void addToQueue(Queue* q, const char* bufferRequest, const char* bufferResponse) {
    Node* newNode = (Node*) malloc(sizeof(Node));
    if (!newNode) {
        exit(EXIT_FAILURE);
        return;
    }
    strcpy(newNode->requestPipe, bufferRequest);
    strcpy(newNode->responsePipe, bufferResponse);
    newNode->next = NULL;

    if (isEmptyQueue(q)) {
        q->head = q->tail = newNode;
        return;
    }

    q->tail->next = newNode;
    q->tail = newNode;

}

void removeHeadQueue(Queue* q) {
    if (isEmptyQueue(q)) {
        return;
    }
    
    Node* temp = q->head;
    q->head = q->head->next;

    if (q->head == NULL) {
        q->tail = NULL;
    }

    free(temp);
}

Node* getHeadQueue(Queue* q) {
    if (isEmptyQueue(q)) {
        return NULL;
    }
    
    return q->head;
}

void freeQueue(Queue* q) {
    if (!q) {
        return;
    }

    Node* current = q->head;
    while (current != NULL) {
        Node* temp = current;
        current = current->next;
        free(temp);
    }

    free(q);
}