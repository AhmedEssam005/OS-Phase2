#include "DataStructures.h"

void initQueue(Queue* q)
{
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
}

void enqueue(Queue* q, PCB* pcb)
{
    QNode* newNode = (QNode*)malloc(sizeof(QNode));
    newNode->pcb = pcb;
    newNode->next = NULL;

    if (q->tail == NULL) {
        q->head = newNode;
        q->tail = newNode;
    } else {
        q->tail->next = newNode;
        q->tail = newNode;
    }
    q->size++;
}

PCB * dequeue(Queue* q)
{
    if (q->head == NULL) {
        return NULL; 
    }
    QNode* temp = q->head;
    PCB* pcb = temp->pcb;
    q->head = q->head->next;
    if (q->head == NULL) {
        q->tail = NULL; 
    }
    free(temp);
    q->size--;
    return pcb;
}

void freeQueue(Queue* q)
{
    while (q->head != NULL) {
        dequeue(q);
    }
}
// steal the last node from q and return the pcb
PCB* steal(Queue* q)
{
    if (q->head == NULL) {
        return NULL; 
    }
    if (q->head == q->tail) {
        PCB* pcb = q->head->pcb;
        free(q->head);
        q->head = NULL;
        q->tail = NULL;
        q->size--;
        return pcb;
    }
    QNode* current = q->head;
    while (current->next != q->tail) {
        current = current->next;
    }
    PCB* pcb = q->tail->pcb;
    free(q->tail);
    current->next = NULL;
    q->tail = current;
    q->size--;
    return pcb;
}


int totalRemainingTime(Queue* q)
{
    int total = 0;
    QNode* current = q->head;
    while (current != NULL) {
        total += current->pcb->remainingTime;
        current = current->next;
    }
    return total;
}

