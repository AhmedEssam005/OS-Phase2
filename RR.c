#include "circQ.h"
#include <math.h>

#define PAGE_FAULT_10 -1
#define PAGE_FAULT_20 -2  
#define MEMORY_HIT 1
#define NO_REQUEST 2

void RR(int msg_id, int sem_id, int total_processes, int quantum, int k) {
    
   
    CircQ *ready_q = initCircQ(100);
    CircQ *blocked_q = initCircQ(100); 

    PCB running_process;
    int finished_processes = 0;
    bool isRunning = false;
    
    FILE *log_file = fopen("scheduler.log", "w");

    int process_sem_id = semget(PROC_SEM_KEY, 1, IPC_CREAT | 0666);
    if (process_sem_id == -1) { perror("semget failed"); exit(-1); }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(process_sem_id, 0, SETVAL, sem_un);

    float total_wta = 0;
    float total_waiting = 0;
    float total_wta_square = 0;
    int total_runtime = 0;
    int current_time = getClk();
    int last_time = -1;
    int context_switch = 0;
    int quantum_counter = 0;
    
    int total_quantums_passed = 0; 

    bool pending_quantum_expire = false;
    PCB expiring_process;

    while (finished_processes < total_processes) {
        current_time = getClk();
        if (current_time > last_time) {
            
            int num_blocked = blocked_q->size;
            for (int i = 0; i < num_blocked; i++) {
                PCB blocked_proc = dequeueCircQ(blocked_q);
                
                if (current_time >= blocked_proc.wakeUpTime) {
                    //TODO: Implement Get the page from disk and update page tables
                    blocked_proc.state = 'S'; 

                    blocked_proc.lastRun = current_time; 
                    
                    enqueueCircQ(ready_q, blocked_proc);
                    fprintf(log_file, "At time %d process %d WOKE UP from Page Fault\n", current_time, blocked_proc.id);
                } else {
                    enqueueCircQ(blocked_q, blocked_proc);
                }
            }

            down(sem_id);
            if (context_switch > 0) {
                context_switch--;
            } else if (isRunning) {
                int relative_time = running_process.runTime - running_process.remainingTime;
                int mem_status = 1; //TODO: Implement memory access and determine if it's a hit or page fault 
                if (mem_status == PAGE_FAULT_10 || mem_status == PAGE_FAULT_20) {
                running_process.state = 'B';
                if (mem_status == PAGE_FAULT_10) {
                    running_process.wakeUpTime = current_time + 10; 
                } else {
                    running_process.wakeUpTime = current_time + 20; 
                }
                
                enqueueCircQ(blocked_q, running_process);
                kill(running_process.pid, SIGSTOP); 
                isRunning = false;
                context_switch = 1;                 
                quantum_counter = 0;                
                
                printf("At time %d process %d PAGE FAULT (Blocked for %d ticks)\n", 
                        current_time, running_process.id, 
                        (mem_status == PAGE_FAULT_10) ? 10 : 20);
            }
                else {
                    running_process.remainingTime--;
                    quantum_counter++;
                    total_runtime++;
                    up(process_sem_id);
                    
                    if (running_process.remainingTime == 0) {
                        int ta = current_time - running_process.arrivalTime;
                        float wta = (float)ta / running_process.runTime;
                        total_wta += wta;
                        total_wta_square += wta * wta;
                        total_waiting += running_process.waitingTime;
                        fprintf(log_file, "At time %d process %d finished arr %d total %d remain 0 wait %d TA %d WTA %.2f\n",
                                current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                                running_process.waitingTime, ta, wta);
                        isRunning = false;
                        context_switch = 1;
                        finished_processes++;
                        quantum_counter = 0;
                    } else if (quantum_counter == quantum) {
                        quantum_counter = 0;
                        total_quantums_passed++;
                        if (total_quantums_passed % k == 0) {
                            // TODO: Implement Clear Reference Bits
                        }
                        expiring_process = running_process;
                        expiring_process.state = 'S';
                        expiring_process.lastRun = current_time;
                        enqueueCircQ(ready_q, expiring_process); 
                        isRunning = false;
                        pending_quantum_expire = true;
                    }
                }
            }
            last_time = current_time;
        }

        ProcessMsg message;

        while(msgrcv(msg_id, &message, sizeof(message) - sizeof(long), PROCESS_MSG_TYPE, IPC_NOWAIT)!=-1){
            PCB new_process;
            new_process.id = message.id;
            new_process.arrivalTime = message.arrival;
            new_process.runTime = message.runtime;
            new_process.priority = message.priority;
            new_process.remainingTime = new_process.runTime;
            new_process.state = 'W';
            new_process.waitingTime = 0;
            new_process.base = message.base;   
            new_process.limit = message.limit; 
            
            enqueueCircQ(ready_q, new_process); 
        }

        if (pending_quantum_expire) {
            if (ready_q->size > 1) { 
                kill(expiring_process.pid, SIGSTOP);
                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        current_time, expiring_process.id, expiring_process.arrivalTime, expiring_process.runTime,
                        expiring_process.remainingTime, expiring_process.waitingTime);
                context_switch = 1;
            } else {
                running_process = dequeueCircQ(ready_q); 
                running_process.state = 'R';
                isRunning = true;
            }
            pending_quantum_expire = false;
        }

        if (ready_q->size > 0 && !isRunning && context_switch == 0) {
            running_process = dequeueCircQ(ready_q);
            quantum_counter = 0;
            if (running_process.state == 'W' && running_process.remainingTime == running_process.runTime) {
                int pid = fork();
                if (pid == 0) {
                    char remStr[10], semStr[10];
                    sprintf(remStr, "%d", running_process.remainingTime);
                    sprintf(semStr, "%d", process_sem_id);
                    execl("./process.out", "process.out", remStr, semStr, NULL);
                    perror("execl process.out failed");
                    exit(-1);
                } else {
                    running_process.startTime = current_time;
                    running_process.pid = pid;
                    running_process.state = 'R';
                    running_process.waitingTime += current_time - running_process.arrivalTime;
                    fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                            current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                            running_process.remainingTime, running_process.waitingTime);
                    isRunning = true;
                }
            } else if (running_process.state == 'S') {
                kill(running_process.pid, SIGCONT);
                running_process.waitingTime += current_time - running_process.lastRun;
                running_process.state = 'R';
                fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        current_time, running_process.id, running_process.arrivalTime, running_process.runTime,
                        running_process.remainingTime, running_process.waitingTime);
                isRunning = true;
            }
        }
    }

    FILE *perf_file = fopen("scheduler.perf", "w");
    float avg_wta = total_wta / total_processes;
    float avg_wait = total_waiting / total_processes;
    float std_dev = sqrt((total_wta_square / total_processes) - (avg_wta * avg_wta));
    float cpu_util = ((float)total_runtime / getClk()) * 100;
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
    fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
    fprintf(perf_file, "Std WTA = %.2f\n", std_dev);
    semctl(process_sem_id, 0, IPC_RMID);
    fclose(log_file);
    fclose(perf_file);
}