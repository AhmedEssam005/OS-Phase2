#ifndef CIRCQ_H
#define CIRCQ_H
#include "DataStructures.h"

typedef struct circQ {
    PCB *buffer;
    int  head;
    int  tail;
    int  size;
    int  capacity;
} CircQ;

CircQ *initCircQ (int capacity);
void   enqueueCircQ   (CircQ *q, PCB item);
PCB    dequeueCircQ   (CircQ *q);
PCB    peekFront (CircQ *q);
int    isEmpty   (CircQ *q);
int    isFull    (CircQ *q);

#endif