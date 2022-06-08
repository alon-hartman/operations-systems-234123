#ifndef QUEUE_H
#define QUEUE_H

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

typedef struct Node_t {
    int            data;
    struct timeval arrival;
    struct Node_t* next;
} Node;

typedef struct {
    Node* head;
    Node* tail;
    int size;
} Queue;

Queue* queueCreate();

void queueDestroy(Queue *q);

int queueEmpty(Queue* q);

int queueSize(Queue* q);

int queuePop(Queue* q, struct timeval *arrival);

int queuePush(Queue* q, int data, struct timeval arrival);

void queuePrint(Queue* q);

void queueDiscardX(Queue* q, int num);

int queueRemoveByIndex(Queue* q , int index);

#endif