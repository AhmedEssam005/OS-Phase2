#include "MMU.h"
#include <limits.h>

FILE *memory_log = NULL;
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

int nru_evict() {
    int victim = -1;
    int best_class = 4;

    for (int i = 0; i < 32; i++) {
        if (RAM[i].is_free || RAM[i].is_PT) {
            continue;
        }

        int r = RAM[i].referenced ? 1 : 0;
        int m = RAM[i].modified ? 1 : 0;
        int cls = r * 2 + m;

        if (cls < best_class) {
            best_class = cls;
            victim = i;
            if (cls == 0) {
                break;
            }
        }
    }

    if (victim == -1) {
        return -1;
    }

    int victim_proc = RAM[victim].occupied_process;
    int victim_vpn = RAM[victim].loaded_VPN;

    // Log swap-out if victim page was modified (spec req 16)
    if (RAM[victim].modified && memory_log) {
        fprintf(memory_log, "Swapping out page %d to disk\n", victim);
        fflush(memory_log);
    }

    // Invalidate victim's PTE
    if (victim_proc >= 0 && victim_proc < 1000 && victim_vpn >= 0) {
        ProcessMemory *victim_mem = &process_memory[victim_proc];
        if (victim_mem->page_table != NULL && victim_vpn < victim_mem->limit) {
            victim_mem->page_table[victim_vpn].valid = false;
            victim_mem->page_table[victim_vpn].PhysicalAddress = -1;
            victim_mem->page_table[victim_vpn].refrenced = 0;
            victim_mem->page_table[victim_vpn].modified = 0;
        }
    }

    // Do NOT set is_free = true — frame stays locked so no other process
    // can steal it while the faulting process is blocked (Arabic note 4)
    RAM[victim].is_free = false;
    RAM[victim].occupied_process = -1;
    RAM[victim].loaded_VPN = -1;
    RAM[victim].referenced = 0;
    RAM[victim].modified = 0;

    return victim;
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
        process_memory[i].base = 0;
        process_memory[i].reserved_frame = -1;
        process_memory[i].pending_fault_vpn = -1;
    }

    memory_log = fopen("memory.log", "w");
    if (memory_log == NULL) {
        perror("ERROR: Unable to open memory.log");
        memory_log = stderr;
    }
    
    memory_initialized = 1;
    printf("[MMU] Initializing MMU: 32 frames allocated, all free\n");
}

void allocate_page_table(int process_id, int limit, int base, int current_time) {
    if (!memory_initialized) {
        fprintf(stderr, "ERROR: MMU not initialized. Call initialize_MMU() first.\n");
        return;
    }
    
    printf("[MMU] Allocating Page Table for Process %d (limit=%d pages, base=%d, time=%d)\n", process_id, limit, base, current_time);
    
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
                    strncpy(process_memory[process_id].requests[idx].binary_addr, binary_addr, sizeof(process_memory[process_id].requests[idx].binary_addr));
                    process_memory[process_id].requests[idx].binary_addr[sizeof(process_memory[process_id].requests[idx].binary_addr) - 1] = '\0';
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
        pt_frame = nru_evict();
        if (pt_frame == -1) {
            fprintf(stderr, "ERROR: No frames available to allocate Page Table for Process %d\n", process_id);
            return;
        }
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
    process_memory[process_id].base = base;
    process_memory[process_id].reserved_frame = -1;
    process_memory[process_id].pending_fault_vpn = -1;
    
    printf("[MMU] Process %d: Page Table allocated at frame %d\n", process_id, pt_frame);
    
    // ========== Part C: Load First Page (VPN 0) ==========
    int first_page_frame = find_free_frame();
    bool found_free = true;
    if (first_page_frame == -1) {
        first_page_frame = nru_evict();
        found_free = false;
        if (first_page_frame == -1) {
            fprintf(stderr, "ERROR: No frames to load first page for Process %d\n", process_id);
            return;
        }
    }

    if (found_free) {
        fprintf(memory_log, "Free Physical page %d allocated\n", first_page_frame);
    }
    fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
            current_time, base + 0, process_id, first_page_frame);
    
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
    // Called when process wakes from blocked queue (Arabic note 3)
    // The frame was already reserved in access_memory() before blocking
    int frame = process_memory[process_id].reserved_frame;
    int vpn = process_memory[process_id].pending_fault_vpn;

    if (frame == -1 || vpn == -1) {
        fprintf(stderr, "ERROR: complete_page_fault called but no reserved frame/vpn for process %d\n", process_id);
        return;
    }

    // Assign the frame to the faulting process
    RAM[frame].is_free = false;
    RAM[frame].is_PT = false;
    RAM[frame].occupied_process = process_id;
    RAM[frame].loaded_VPN = vpn;
    RAM[frame].referenced = 1;  // Page is being accessed now
    RAM[frame].modified = 0;

    // Update the faulting process's PTE
    process_memory[process_id].page_table[vpn].PhysicalAddress = frame;
    process_memory[process_id].page_table[vpn].valid = true;
    process_memory[process_id].page_table[vpn].refrenced = 0;
    process_memory[process_id].page_table[vpn].modified = 0;

    // Log: At time T disk address D for process P is loaded into memory page F.
    // disk_address = base + vpn
    int disk_address = process_memory[process_id].base + vpn;
    if (memory_log) {
        fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
                current_time, disk_address, process_id, frame);
        fflush(memory_log);
    }

    // Reset reservation (Arabic note 4: flag back to -1 when process resumes)
    process_memory[process_id].reserved_frame = -1;
    process_memory[process_id].pending_fault_vpn = -1;

    printf("[MMU] Process %d completed page fault at time %d: VPN %d -> frame %d\n",
           process_id, current_time, vpn, frame);
}

void clear_all_r_bits() {
    // Called every K quantums to make NRU meaningful (spec NRU Notes)
    for (int i = 0; i < 32; i++) {
        if (RAM[i].is_free || RAM[i].is_PT) {
            continue;
        }
        // Clear R bit in frame
        RAM[i].referenced = 0;

        // Also clear R bit in the owning process's PTE
        int proc = RAM[i].occupied_process;
        int vpn = RAM[i].loaded_VPN;
        if (proc >= 0 && proc < 1000 && vpn >= 0) {
            if (process_memory[proc].page_table != NULL && vpn < process_memory[proc].limit) {
                process_memory[proc].page_table[vpn].refrenced = 0;
            }
        }
    }
    printf("[MMU] Cleared all R-bits for NRU algorithm\n");
}

void free_process_memory(int process_id) {
    // Free all frames owned by this process (both data and PT frames)
    for (int i = 0; i < 32; i++) {
        if (RAM[i].occupied_process == process_id && !RAM[i].is_free) {
            RAM[i].is_free = true;
            RAM[i].is_PT = false;
            RAM[i].occupied_process = -1;
            RAM[i].loaded_VPN = -1;
            RAM[i].referenced = 0;
            RAM[i].modified = 0;
        }
    }

    // Free dynamically allocated arrays
    if (process_memory[process_id].page_table != NULL) {
        free(process_memory[process_id].page_table);
        process_memory[process_id].page_table = NULL;
    }
    if (process_memory[process_id].requests != NULL) {
        free(process_memory[process_id].requests);
        process_memory[process_id].requests = NULL;
    }

    // Reset ProcessMemory entry
    process_memory[process_id].process_id = -1;
    process_memory[process_id].limit = 0;
    process_memory[process_id].base = 0;
    process_memory[process_id].request_count = 0;
    process_memory[process_id].last_request_idx = 0;
    process_memory[process_id].reserved_frame = -1;
    process_memory[process_id].pending_fault_vpn = -1;

    printf("[MMU] Freed all memory for process %d\n", process_id);
}