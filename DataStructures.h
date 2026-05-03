#ifndef DATASTRUCTURES_H
#define DATASTRUCTURES_H

#include "headers.h"


/* =========================== Process Control Block =============================*/
typedef struct 
{
    int id; //process id from file
    pid_t pid; //process id in the system
    int arrivalTime;
    int runTime;
    int priority;
    int waitingTime;
    int remainingTime;
    int startTime;
    int finishTime;
    int lastRun; //the last time the process was running, used to calculate waiting time
    int cpuid; // the cpu that the process is running on, used for performance calculations
    char state; //R, W, S, F, B
    int PT_PhysicalAddress;
    int base; 
    int limit; 
    int wakeUpTime;
} PCB;
typedef struct{
    int occupied_process;
    int loaded_VPN;
    int referenced;
    int modified;
    bool is_free;
    bool is_PT;
} Frame;

typedef struct{
    int PhysicalAddress;
    bool valid;
    int refrenced;
    int modified;
} PTE;

/* ============================================================================== */
/* ================ Shared Memory Structure ================ */
typedef struct {
    int remaining1;         // scheduler1 writes this
    PCB stolen_process;     // the stolen PCB placed here
    int steal_ready;        // 1 when stolen process is placed in shm
    int barrier_count;      // counter for barrier synchronization
    int steal_checkpoint;   // the simulated time when stealing should happen
    int cpu1_arrived;       // CPU1 has arrived at this checkpoint time
    int cpu1_finished;
} StealShm;

/* ==================== FCFS Queue for one and two CPUs ========================= */
typedef struct QNode
{
    PCB  * pcb;
    struct QNode* next;
} QNode;

typedef struct Queue
{
    struct QNode* head;
    struct QNode* tail;
    int size;
} Queue;

void initQueue(Queue* q);
void enqueue(Queue* q, PCB* pcb);
PCB * dequeue(Queue* q);
void freeQueue(Queue* q);
// sum of the remaining time of all processes in the queue
int totalRemainingTime(Queue* q);


#endif

/* ======================================================================= */