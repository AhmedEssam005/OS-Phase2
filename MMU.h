#ifndef MMU_H
#define MMU_H
#include "DataStructures.h"
#define PAGE_FAULT_10 -1  
#define PAGE_FAULT_20 -2  
#define MEMORY_HIT 1
#define NO_REQUEST 2

extern FILE *memory_log;

void initialize_MMU();
void allocate_page_table(PCB *pcb, int current_time);
int access_memory(PCB *pcb, int relative_time, int current_time);
void complete_page_fault(PCB *pcb, int current_time);
void clear_all_r_bits();
int nru_evict(bool *is_modified);
void free_process_memory(PCB *pcb);

#endif