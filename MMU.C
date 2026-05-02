#include "MMU.h"
int pending_faults[100];
Frame RAM[32];

void initialize_MMU() {
    // TODO: Loop through RAM[0] to RAM[31]
    // TODO: Set is_free = 1 for all frames
    // TODO: Set is_page_table = 0 for all frames
    // TODO: Reset process_id, virtual_page_number, r_bit, and m_bit to 0
    printf("[MMU DUMMY] Initializing MMU (Setting all frames to free)\n");
}


void allocate_page_table(int process_id, int limit) {
    // TODO: Find 1 free frame in RAM to store this process's Page Table
    // TODO: Mark that frame as occupied (is_free = 0) and is_page_table = 1
    // TODO: Initialize an array of PTEs (size = limit) inside that frame
    
    // TODO: Open the file "requests<process_id>.txt"
    // TODO: Read all lines and store the requests (time, address, R/W) in a struct array so access_memory can check it quickly
    
    printf("[MMU DUMMY] Allocating Page Table for Process %d with limit %d\n", process_id, limit);
}


int access_memory(int process_id, int relative_time, int current_time) {
    // TODO: Read request.
    // TODO: If NO_REQUEST or MEMORY_HIT, return it.
    
    // TODO: If PAGE_FAULT occurs:
    //       1. Check if RAM is full.
    //       2. If full, run NRU immediately to find a victim.
    //       3. If victim's m_bit == 1, return PAGE_FAULT_20.
    //       4. Else, return PAGE_FAULT_10.
    return NO_REQUEST; 
}

void complete_page_fault(int process_id, int current_time) {
    // TODO: Find a free frame in RAM for the incoming page
    // TODO: If NO free frames exist -> Run the NRU Eviction Algorithm!
    //       -> (NRU: Check classes 0-3 based on R/M bits, evict a victim, update victim's PTE to is_valid = 0)
    
    // TODO: Assign the frame to the faulting process
    // TODO: Update the frame's metadata (process_id, virtual_page_number, is_free = 0)
    // TODO: Update the faulting process's PTE (is_valid = 1, frame_number = X)
    
    printf("[MMU DUMMY] Process %d completed page fault at time %d\n", process_id, current_time);
}

void clear_all_r_bits() {
    // TODO: Loop through RAM[0] to RAM[31]
    // TODO: If a frame is occupied (!is_free) AND is not a page table (!is_page_table):
    //       -> Set frame's r_bit = 0
    //       -> Find the owning process's PTE for this frame and set its r_bit = 0
    
    printf("[MMU DUMMY] Clearing all R-bits for NRU algorithm\n");
}