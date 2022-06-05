#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

// typedef struct Node_t {
//     int data;
//     struct Node_t* next;
// } Node;

// typedef struct {
//     Node* head;
//     Node* tail;
//     int size;
// } Queue;

Queue* queueCreate() {
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

void queueDestroy(Queue *q) {
    Node *temp, *it = q->head;
    while(it) {
        temp = it;
        it = it->next;
        free(temp);
    }
    free(q);
}

int queueEmpty(Queue* q) {
    if(q->head == NULL){
        return 1;
    }
    return 0;
}

int queueSize(Queue* q) {
    return q->size;
}

int queuePop(Queue* q) {
    if(queueEmpty(q)) {
        return -1;  // error
    }
    int data = q->head->data;
    Node* temp = q->head;
    q->head = q->head->next;
    free(temp);
    q->size--;
    if(q->head == NULL) {
        q->tail = NULL;
    }
    return data;
}

int queuePush(Queue* q, int data) {
    Node* new_node = (Node *)malloc(sizeof(Node));
    if(!new_node) {
        return -1;
    }
    new_node->data = data;
    new_node->next = NULL;
    if(queueEmpty(q) == 0) {//queue NOT empty
        q->tail->next = new_node;
        q->tail = new_node;
    }
    else {
        q->tail = new_node;
        q->head = new_node;
    }
    q->size++;
    return 1;
}

void printQueue(Queue* q) {
    Node* it = q->head;
    printf("queue:    ");
    while(it) {
        printf("%d ", it->data);
    }
}
