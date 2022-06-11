#include "queue.h"
#include <stdlib.h>
#include <stdio.h>
#include "segel.h"

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
        Close(it->data);
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

int queuePop(Queue* q, struct timeval* at) {
    if(queueEmpty(q)) {
        return -1;  // error
    }
    int data = q->head->data;
    Node* temp = q->head;
    if(at != NULL){
        *at = temp->arrival; 
    }
    q->head = q->head->next;
    free(temp);
    q->size--;
    if(q->head == NULL) {
        q->tail = NULL;
    }
    return data;
}

int queuePush(Queue* q, int data, struct timeval arrival) {
    Node* new_node = (Node *)malloc(sizeof(Node));
    if(!new_node) {
        return -1;
    }
    new_node->data = data;
    new_node->arrival = arrival;
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

void queuePrint(Queue* q) {
    Node* it = q->head;
    if(q->size == 0) {
        printf("queue is empty\n");
        return;
    }
    printf("queue:    ");
    while(it) {
        printf("%d ", it->data);
        it = it->next;
    }
}

void queueDiscardX(Queue* q, int num) {
    num = (num < queueSize(q)) ? num : queueSize(q);
    while(num > 0) {
        int index = rand() % q->size;
        int p = queueRemoveByIndex(q, index);
        Close(p);
        num--;
    }
}

int queueRemoveByIndex(Queue* q, int index) {
    if(index > q->size) {
        return -1;
    }
    if(index == 0) {
        return queuePop(q,NULL);
    }
    Node* it = q->head;
    Node* tmp;int fd;
    for (int i = 0; i < index-1; i++) {
        it = it->next;
    }
    tmp = it->next;
    it->next = it->next->next;
    fd = tmp->data;
    free(tmp);
    if(it->next == NULL) {
        q->tail = it;
    }
    q->size--;
    return fd;
}
