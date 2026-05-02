#include "headers.h"

/* Modify this file as needed*/
int remainingtime;

int main(int agrc, char * argv[])
{
    // scheduler forks us with only 1 argument: remaining time
    // execl("process.out", "process.out", remainingTimeStr, NULL)
    remainingtime = atoi(argv[1]);
    int sem_id = atoi(argv[2]);

    initClk();
    
    while (remainingtime > 0)
    {
       down(sem_id);
       remainingtime--;
    }
    
    destroyClk(false);
    
    return 0;
}