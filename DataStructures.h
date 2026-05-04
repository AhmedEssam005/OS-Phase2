#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include "headers.h"
typedef struct
{
    int time;
    int address;
    char binary_addr[16];
    char rwFlag;
} Request;

typedef struct
{
    int id;    
    pid_t pid; 
    int arrivalTime;
    int runTime;
    int priority;
    int waitingTime;
    int remainingTime;
    int startTime;
    int finishTime;
    int lastRun;
    int cpuid;
    char state;
    int PT_PhysicalAddress;
    int base;
    int limit;
    int wakeUpTime;
    Request *requests;     
    int request_count;     
    int last_request_idx;  
    int reserved_frame;    
    int pending_fault_vpn;
} PCB;

typedef struct
{
    int PhysicalAddress;
    bool valid;
    int refrenced;
    int modified;
} PTE;
typedef struct
{
    int occupied_process;
    int loaded_VPN;
    int referenced;
    int modified;
    bool is_free;
    bool is_PT;
    PTE* page_table;
    int pt_limit;
} Frame;



typedef struct QNode
{
    PCB *pcb;
    struct QNode *next;
} QNode;

typedef struct Queue
{
    struct QNode *head;
    struct QNode *tail;
    int size;
} Queue;

void initQueue(Queue *q);
void enqueue(Queue *q, PCB *pcb);
PCB *dequeue(Queue *q);
void freeQueue(Queue *q);
int totalRemainingTime(Queue *q);

#endif
