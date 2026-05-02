#include "circQ.h"

CircQ *initCircQ(int capacity) {
    CircQ *q    = (CircQ *)malloc(sizeof(CircQ));
    q->buffer   = (PCB *)malloc(capacity * sizeof(PCB));
    q->head     = 0;
    q->tail     = 0;
    q->size     = 0;
    q->capacity = capacity;
    return q;
}

int isEmpty(CircQ *q) {
    return q->size == 0;
}

int isFull(CircQ *q) {
    return q->size == q->capacity;
}

void enqueueCircQ(CircQ *q, PCB item) {
    if (isFull(q)) {
        printf("Circular Queue is full!\n");
        return;
    }
    q->buffer[q->tail] = item;
    q->tail            = (q->tail + 1) % q->capacity;
    q->size++;
}

PCB dequeueCircQ(CircQ *q) {
    if (isEmpty(q)) {
        PCB empty = {0};
        return empty;
    }
    PCB item = q->buffer[q->head];
    q->head  = (q->head + 1) % q->capacity;
    q->size--;
    return item;
}

PCB peekFront(CircQ *q) {
    if (isEmpty(q)) {
        PCB empty = {0};
        return empty;
    }
    return q->buffer[q->head];
}