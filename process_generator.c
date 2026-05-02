#include "DataStructures.h"
#include "headers.h"
#define MAX_PROCESSES 1000

typedef struct {
    int arrivaltime;
    int priority;
    int runningtime;
    int id;
    int base;  
    int limit; 
} processData;


processData processes[MAX_PROCESSES];
int process_count = 0;
int msgqid = -1;
int msgqid1 = -1;
int msgqid2 = -1; 
int sem_id = -1;

void clearResources(int signum);

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);

    FILE *f = fopen("processes.txt", "r");
    if (f == NULL) {
        perror("Cannot open processes.txt — run test_generator first!");
        exit(-1);
    }

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        if (line[0] == '#') continue;

        processData p = {0}; 
        if (sscanf(line, "%d\t%d\t%d\t%d\t%d\t%d",
                   &p.id, &p.arrivaltime, &p.runningtime, &p.priority, &p.base, &p.limit) == 6)
        {
            processes[process_count++] = p;
        }
    }
    fclose(f);
    printf("Read %d processes.\n", process_count);

    int algo = 2, quantum = 0, k_timeout = 0;

    printf("\nUsing Round Robin (RR) scheduling algorithm.\n");
    printf("Enter quantum: ");
    scanf("%d", &quantum);
    
    printf("Enter K timeout for NRU: "); 
    scanf("%d", &k_timeout);

    key_t key1 = ftok("keyfile", MSGKEY1);
    key_t key2 = ftok("keyfile", MSGKEY2);
    if(key1 == -1 || key2 == -1) { perror("ftok failed"); exit(-1); }
    msgqid1 = msgget(key1, IPC_CREAT | 0666);
    msgqid2 = msgget(key2, IPC_CREAT | 0666);
    if (msgqid1 == -1 || msgqid2 == -1) { perror("msgget failed"); exit(-1); }
    msgqid = msgqid1;


    sem_id = semget(SEMKEY, 1, IPC_CREAT | 0666);
    if(sem_id == -1) { perror("semget failed"); exit(-1); }
    union Semun sem_un;
    sem_un.val = 0;
    semctl(sem_id, 0, SETVAL, sem_un);
   

   
    pid_t clk_pid = fork();
    if (clk_pid == -1) { perror("fork clock failed"); exit(-1); }
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        perror("execl clock failed");
        exit(-1);
    }
  
   
    initClk();            

    char algo_s[10], quantum_s[10], total_s[10], k_s[10]; 
    sprintf(algo_s,    "%d", algo);
    sprintf(quantum_s, "%d", quantum);
    sprintf(total_s,   "%d", process_count);
    sprintf(k_s,       "%d", k_timeout); 

   pid_t sch1_pid = -1;

    sch1_pid = fork();
    if(sch1_pid == -1) { perror("fork scheduler1"); exit(-1); }
    if(sch1_pid == 0) {
        execl("./scheduler.out","scheduler.out","1",algo_s,total_s,quantum_s, k_s, NULL);
        perror("execl sch1"); exit(1);
    }
   
    int sent = 0;
    int prev_clk = -1;
    bool end_sent = false;
    while(true) {
        int now = getClk();
        if(now == prev_clk)  continue; 
        prev_clk = now;

        for(int i = 0; i < process_count; i++) {
            if(processes[i].arrivaltime != now) continue;

            ProcessMsg message;
            message.mtype    = PROCESS_MSG_TYPE;
            message.id       = processes[i].id;
            message.arrival  = processes[i].arrivaltime;
            message.runtime  = processes[i].runningtime;
            message.priority = processes[i].priority;
            message.base     = processes[i].base;
            message.limit    = processes[i].limit;

            msgsnd(msgqid1, &message, sizeof(ProcessMsg)-sizeof(long), 0);
            printf("Sent process %d at time %d\n", processes[i].id, now);
            sent++;
        }

        
        up(sem_id);
        if(sent >= process_count && !end_sent) {
            end_sent = true;
            ProcessMsg end_msg;
            end_msg.mtype    = END_OF_STREAM_MSG_TYPE;
            end_msg.id       = -1;
            end_msg.arrival  = -1;
            end_msg.runtime  = -1;
            end_msg.priority = -1;
            end_msg.base     = -1;
            end_msg.limit    = -1;

            msgsnd(msgqid1, &end_msg, sizeof(ProcessMsg)-sizeof(long), 0);
        }

        if(end_sent) {
            if(waitpid(sch1_pid, NULL, WNOHANG) > 0)
                break;
        }
    }
    printf("At time %d: All processes sent and schedulers finished. Cleaning up resources...\n", getClk());
    clearResources(0);
    return 0;
}

void clearResources(int signum)
{
    signal(SIGINT, SIG_DFL); 
    if (msgqid1 != -1) {
        msgctl(msgqid1, IPC_RMID, NULL); 
        printf("Message queue 1 removed.\n");
    }
    if (msgqid2 != -1) {
        msgctl(msgqid2, IPC_RMID, NULL); 
        printf("Message queue 2 removed.\n");
    }
    
    destroyClk(true);
}