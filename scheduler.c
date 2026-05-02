#include "headers.h"
#include "DataStructures.h"
void RR (int msg_id, int sem_id, int total_processes, int quantum, int k);

PCB pcbTable[1000]; // assuming maximum 100 processes
int pcbTableSize = 0;  // how many processes in the table so far


int main(int argc, char * argv[])
{
    initClk();
    signal(SIGUSR1, SIG_IGN);
    
    if(argc < 3) {
        fprintf(stderr, "Usage: %s <cpu_id> <algorithm> <total_processes> [parameters]\n", argv[0]);
        exit(1);
    }
    
    int cpu_id = atoi(argv[1]);
    int algo = atoi(argv[2]);
    int TotalProcesses = atoi(argv[3]);
    int quantum = argc > 4 ? atoi(argv[4]) : 0;
    int k = argc > 5 ? atoi(argv[5]) : 0;
    
    if(algo == ALGO_RR && argc < 5) {
        fprintf(stderr, "Usage for RR: %s <cpu_id> 2 <total> <quantum>\n", argv[0]);
        exit(1);
    }

    key_t key = ftok("keyfile", cpu_id == 1 ? MSGKEY1 : MSGKEY2);
    if(key == -1) { perror("ftok failed"); exit(-1); }
    int msqid = msgget(key, 0666);
    if(msqid == -1) {
        perror("msgget failed");
        exit(1);
    }
    
    int sem_id = semget(SEMKEY, 1, 0666);
    if(sem_id == -1) { perror("semget sem failed"); exit(1); }
    
    
    switch(algo) {
        case ALGO_RR:
            RR(msqid, sem_id, TotalProcesses, quantum, k);
            break;
        default:
            fprintf(stderr, "Unknown algorithm ID: %d\n", algo);
            exit(1);
    }
    
    return 0;
}
