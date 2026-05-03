#ifndef HEADERS_H
#define HEADERS_H

#include <stdio.h>      
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#if defined(__STDC_VERSION__) || defined(__cplusplus)
#include <stdbool.h>
#else
typedef short bool;
#define true 1
#define false 0
#endif

#define SHKEY 300

/* ======== Message queue keys (generator <-> schedulers) ============= */         
#define MSGKEY 200      
#define MSGKEY1 65      // queue for scheduler 1

///////////////////
#define SEMKEY 100

#define SEMKEY_clk 103

#define PROC_SEM_KEY 400

/* ======== Semaphore indices within steal semaphore set ============= */
#define SEM_BARRIER  0   // both schedulers reached tick N
#define SEM_REQUEST  1   // coordinator signals worker to send process
#define SEM_DONE     2   // worker signals coordinator that process is in shm  
union Semun{
    int val;               
    struct semid_ds *buf;  
    unsigned short *array; 
    struct seminfo *__buf; 
};



static void up(int semid){
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = 0;
    semop(semid, &op, 1);
}

static void down(int semid){
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = 0;
    semop(semid, &op, 1);
}

static void up_n(int semid, int n) {
    struct sembuf op = {n, 1, 0};
    semop(semid, &op, 1);
}

static void down_n(int semid, int n) {
    struct sembuf op = {n, -1, 0};
    semop(semid, &op, 1);
}
/* ==================== Algorithm IDs ================================= */
#define ALGO_RR   2

/* =========================================== */
typedef struct {
    long mtype;    
    int  id;
    int  arrival;
    int  runtime;
    int  priority;
    int base;
    int limit;
} ProcessMsg;

/* Special message types */
#define PROCESS_MSG_TYPE 1
#define END_OF_STREAM_MSG_TYPE 2



///==============================
//don't mess with this variable//
extern int * shmaddr;                 //
//===============================


int getClk();

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk();

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll);

#endif
