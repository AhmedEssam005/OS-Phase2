#include "MMU.h"
#include <limits.h>

int pending_faults[100];
Frame RAM[32];
ProcessMemory process_memory[1000];
int memory_initialized = 0;

int binary_to_decimal(const char *binary_str) {
    if (binary_str == NULL || binary_str[0] == '\0') {
        return 0;
    }
    
    int result = 0;
    int i = 0;
    
    while (binary_str[i] != '\0') {
        if (binary_str[i] != '0' && binary_str[i] != '1') {
            fprintf(stderr, "ERROR: Invalid binary character '%c' in '%s'\n", binary_str[i], binary_str);
            return -1;
        }
        result = result * 2 + (binary_str[i] - '0');
        i++;
    }
    
    if (result > 1023) {
        fprintf(stderr, "ERROR: Binary value exceeds 10-bit range: %d\n", result);
        return -1;
    }
    
    return result;
}

int find_free_frame() {
    for (int i = 0; i < 32; i++) {
        if (RAM[i].is_free) {
            return i;
        }
    }
    return -1;  // No free frames
}


void initialize_MMU() {
    // Initialize all 32 frames as free
    for (int i = 0; i < 32; i++) {
        RAM[i].is_free = true;
        RAM[i].is_PT = false;
        RAM[i].occupied_process = -1;
        RAM[i].loaded_VPN = -1;
        RAM[i].referenced = 0;
        RAM[i].modified = 0;
    }
    
    // Initialize process memory array
    for (int i = 0; i < 1000; i++) {
        process_memory[i].process_id = -1;
        process_memory[i].limit = 0;
        process_memory[i].page_table = NULL;
        process_memory[i].requests = NULL;
        process_memory[i].request_count = 0;
        process_memory[i].last_request_idx = 0;
    }
    
    memory_initialized = 1;
    printf("[MMU] Initializing MMU: 32 frames allocated, all free\n");
}

void allocate_page_table(int process_id, int limit) {
    if (!memory_initialized) {
        fprintf(stderr, "ERROR: MMU not initialized. Call initialize_MMU() first.\n");
        return;
    }
    
    printf("[MMU] Allocating Page Table for Process %d (limit=%d pages)\n", process_id, limit);
    
    // ========== Part A: Load Request File ==========
    char filename[256];
    snprintf(filename, sizeof(filename), "requests%d.txt", process_id);
    
    FILE *req_file = fopen(filename, "r");
    if (req_file == NULL) {
        printf("[MMU] WARNING: Request file '%s' not found. Process %d will have no requests.\n", filename, process_id);
        process_memory[process_id].request_count = 0;
        process_memory[process_id].requests = NULL;
    } else {
        // Count non-comment lines first
        int request_count = 0;
        char line[256];
        while (fgets(line, sizeof(line), req_file)) {
            // Skip empty lines and comments
            if (line[0] == '\n' || line[0] == '#') continue;
            request_count++;
        }
        
        // Allocate memory for requests
        if (request_count > 0) {
            process_memory[process_id].requests = (Request *)malloc(request_count * sizeof(Request));
            if (process_memory[process_id].requests == NULL) {
                fprintf(stderr, "ERROR: Failed to allocate memory for requests\n");
                fclose(req_file);
                return;
            }
        }
        
        // Read requests again
        rewind(req_file);
        int idx = 0;
        while (fgets(line, sizeof(line), req_file)) {
            // Skip empty lines and comments
            if (line[0] == '\n' || line[0] == '#') continue;
            
            int time;
            char binary_addr[32];
            char rw_flag;
            
            // Parse: time <space> address <space> r/w
            if (sscanf(line, "%d %s %c", &time, binary_addr, &rw_flag) == 3) {
                int address = binary_to_decimal(binary_addr);
                if (address >= 0) {  // Valid conversion
                    process_memory[process_id].requests[idx].time = time;
                    process_memory[process_id].requests[idx].address = address;
                    process_memory[process_id].requests[idx].rwFlag = rw_flag;
                    idx++;
                }
            }
        }
        
        process_memory[process_id].request_count = idx;
        fclose(req_file);
        printf("[MMU] Process %d: Loaded %d memory requests from '%s'\n", process_id, idx, filename);
    }
    
    // ========== Part B: Initialize Page Table ==========
    // Find a free frame for the Page Table itself
    int pt_frame = find_free_frame();
    if (pt_frame == -1) {
        fprintf(stderr, "ERROR: No free frames available to allocate Page Table for Process %d\n", process_id);
        return;  // Person 3 will handle NRU eviction in complete version
    }
    
    // Allocate Page Table Entry array
    PTE *page_table = (PTE *)malloc(limit * sizeof(PTE));
    if (page_table == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for Page Table of Process %d\n", process_id);
        return;
    }
    
    // Initialize all PTEs as invalid
    for (int i = 0; i < limit; i++) {
        page_table[i].PhysicalAddress = -1;
        page_table[i].valid = false;
        page_table[i].refrenced = 0;  // Note: typo in original DataStructures.h kept as-is
        page_table[i].modified = 0;
    }
    
    // Mark the PT frame as occupied and reserved for page table
    RAM[pt_frame].is_free = false;
    RAM[pt_frame].is_PT = true;
    RAM[pt_frame].occupied_process = process_id;
    RAM[pt_frame].loaded_VPN = -1;  // Not a data page
    RAM[pt_frame].referenced = 1;   // PT accessed
    RAM[pt_frame].modified = 0;     // Not modified yet
    
    // Store in process memory array
    process_memory[process_id].process_id = process_id;
    process_memory[process_id].limit = limit;
    process_memory[process_id].page_table = page_table;
    process_memory[process_id].last_request_idx = 0;
    
    printf("[MMU] Process %d: Page Table allocated at frame %d\n", process_id, pt_frame);
    
    // ========== Part C: Load First Page (VPN 0) ==========
    int first_page_frame = find_free_frame();
    if (first_page_frame == -1) {
        fprintf(stderr, "ERROR: No free frames to load first page for Process %d\n", process_id);
        // In complete version, Person 3 handles NRU eviction here
        return;
    }
    
    // Mark the data frame as occupied
    RAM[first_page_frame].is_free = false;
    RAM[first_page_frame].is_PT = false;
    RAM[first_page_frame].occupied_process = process_id;
    RAM[first_page_frame].loaded_VPN = 0;
    RAM[first_page_frame].referenced = 1;  // Loaded
    RAM[first_page_frame].modified = 0;    // Not modified
    
    // Update PTE[0] to mark first page as valid and loaded
    page_table[0].PhysicalAddress = first_page_frame;
    page_table[0].valid = true;
    page_table[0].refrenced = 0;
    page_table[0].modified = 0;
    
    printf("[MMU] Process %d: First page (VPN 0) allocated at frame %d\n", process_id, first_page_frame);
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
    
    printf("[MMU] Process %d completed page fault at time %d\n", process_id, current_time);
}

void clear_all_r_bits() {
    // TODO: Loop through RAM[0] to RAM[31]
    // TODO: If a frame is occupied (!is_free) AND is not a page table (!is_page_table):
    //       -> Set frame's r_bit = 0
    //       -> Find the owning process's PTE for this frame and set its r_bit = 0
    
    printf("[MMU DUMMY] Clearing all R-bits for NRU algorithm\n");
}