#include "circQ.h"
#include <math.h>
#include "MMU.h"

#define PAGE_FAULT_10 -1
#define PAGE_FAULT_20 -2
#define MEMORY_HIT 1
#define NO_REQUEST 2

void RR(int msg_id, int sem_id, int total_processes, int quantum, int k)
{

    CircQ *ready_q = initCircQ(100);
    CircQ *blocked_q = initCircQ(100);

    PCB running_process;
    int finished_processes = 0;
    bool isRunning = false;

    int process_sem_id = semget(PROC_SEM_KEY, 1, IPC_CREAT | 0666);
    if (process_sem_id == -1)
    {
        perror("semget failed");
        exit(-1);
    }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(process_sem_id, 0, SETVAL, sem_un);

    int total_runtime = 0;
    int current_time = getClk();
    int last_time = -1;
    int context_switch = 0;
    int quantum_counter = 0;

    int total_quantums_passed = 0;

    bool pending_quantum_expire = false;
    bool generator_finished = false;
    PCB expiring_process;

    initialize_MMU();

    while (!generator_finished || isRunning || ready_q->size > 0 || blocked_q->size > 0)
    {
        current_time = getClk();
        if (current_time > last_time)
        {

            down(sem_id);
            if (context_switch > 0)
            {
                context_switch--;
            }
            else if (isRunning)
            {
                int relative_time = running_process.runTime - running_process.remainingTime;
                int mem_status = 1;
                mem_status = access_memory(&running_process, relative_time, current_time);
                if (mem_status == PAGE_FAULT_10 || mem_status == PAGE_FAULT_20)
                {
                    running_process.state = 'B';
                    if (mem_status == PAGE_FAULT_10)
                    {
                        running_process.wakeUpTime = current_time + 10;
                    }
                    else
                    {
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
                else
                {
                    running_process.remainingTime--;
                    quantum_counter++;
                    total_runtime++;
                    up(process_sem_id);

                    if (running_process.remainingTime == 0)
                    {

                        isRunning = false;
                        context_switch = 1;
                        finished_processes++;
                        quantum_counter = 0;
                        total_quantums_passed++;
                        if (total_quantums_passed % k == 0)
                        {
                            clear_all_r_bits();
                        }
                        free_process_memory(&running_process);
                    }
                    else if (quantum_counter == quantum)
                    {
                        quantum_counter = 0;
                        total_quantums_passed++;
                        if (total_quantums_passed % k == 0)
                        {
                            clear_all_r_bits();
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
            int num_blocked = blocked_q->size;
            for (int i = 0; i < num_blocked; i++)
            {
                PCB blocked_proc = dequeueCircQ(blocked_q);

                if (current_time >= blocked_proc.wakeUpTime)
                {
                    complete_page_fault(&blocked_proc, current_time);
                    blocked_proc.state = 'S';

                    blocked_proc.lastRun = current_time;

                    enqueueCircQ(ready_q, blocked_proc);
                }
                else
                {
                    enqueueCircQ(blocked_q, blocked_proc);
                }
            }
            last_time = current_time;
        }

        ProcessMsg message;

        while (msgrcv(msg_id, &message, sizeof(message) - sizeof(long), 0, IPC_NOWAIT) != -1)
        {
            if (message.id == -1)
            {
                generator_finished = true;
                printf("At time %d: Process generator has finished sending all processes.\n", current_time);
                continue;
            }

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

        if (pending_quantum_expire)
        {
            if (ready_q->size > 1)
            {
                kill(expiring_process.pid, SIGSTOP);
                context_switch = 1;
            }
            else
            {
                running_process = dequeueCircQ(ready_q);
                running_process.state = 'R';
                isRunning = true;
            }
            pending_quantum_expire = false;
        }

        if (ready_q->size > 0 && !isRunning && context_switch == 0)
        {
            running_process = dequeueCircQ(ready_q);
            quantum_counter = 0;
            if (running_process.state == 'W' && running_process.remainingTime == running_process.runTime)
            {
                int pid = fork();
                if (pid == 0)
                {
                    char remStr[10], semStr[10];
                    sprintf(remStr, "%d", running_process.remainingTime);
                    sprintf(semStr, "%d", process_sem_id);
                    execl("./process.out", "process.out", remStr, semStr, NULL);
                    perror("execl process.out failed");
                    exit(-1);
                }
                else
                {
                    running_process.startTime = current_time;
                    running_process.pid = pid;
                    running_process.state = 'R';
                    running_process.waitingTime += current_time - running_process.arrivalTime;
                    allocate_page_table(&running_process, current_time);
                    isRunning = true;
                }
            }
            else if (running_process.state == 'S')
            {
                kill(running_process.pid, SIGCONT);
                running_process.waitingTime += current_time - running_process.lastRun;
                running_process.state = 'R';
                isRunning = true;
            }
        }
    }

    semctl(process_sem_id, 0, IPC_RMID);
    fclose(memory_log);
}