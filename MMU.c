#include "MMU.h"
#include <limits.h>

FILE *memory_log = NULL;
Frame RAM[32];
int memory_initialized = 0;

int binary_to_decimal(const char *binary_str)
{
    if (binary_str == NULL || binary_str[0] == '\0')
    {
        return 0;
    }

    int result = 0;
    int i = 0;

    while (binary_str[i] != '\0')
    {
        if (binary_str[i] != '0' && binary_str[i] != '1')
        {
            printf("ERROR: Invalid binary character '%c' in '%s'\n", binary_str[i], binary_str);
            return -1;
        }
        result = result * 2 + (binary_str[i] - '0');
        i++;
    }

    if (result > 1023)
    {
        printf("ERROR: Binary value exceeds 10-bit range: %d\n", result);
        return -1;
    }

    return result;
}

void initialize_MMU()
{
    for (int i = 0; i < 32; i++)
    {
        RAM[i].is_free = true;
        RAM[i].is_PT = false;
        RAM[i].occupied_process = -1;
        RAM[i].loaded_VPN = -1;
        RAM[i].referenced = 0;
        RAM[i].modified = 0;
        RAM[i].page_table = NULL;
        RAM[i].pt_limit = 0;
    }

    memory_log = fopen("memory.log", "w");
    if (memory_log == NULL)
    {
        perror("ERROR: Unable to open memory.log");
        memory_log = stderr;
    }

    memory_initialized = 1;
    printf("[MMU] Initializing MMU: 32 frames allocated, all free\n");
}

int find_free_frame()
{
    for (int i = 0; i < 32; i++)
    {
        if (RAM[i].is_free)
        {
            if (memory_log)
            {
                fprintf(memory_log, "Free Physical page %d allocated\n", i);
                fflush(memory_log);
            }
            return i;
        }
    }
    return -1;
}

void allocate_page_table(PCB *pcb, int current_time)
{
    if (!memory_initialized)
        return;

    printf("[MMU] Allocating Page Table for Process %d\n", pcb->id);

    char filename[256];
    snprintf(filename, sizeof(filename), "requests_%d.txt", pcb->id);
    FILE *req_file = fopen(filename, "r");
    if (req_file == NULL)
    {
        pcb->request_count = 0;
        pcb->requests = NULL;
    }

    else
    {
        int count = 0;
        char line[256];
        while (fgets(line, sizeof(line), req_file))
            if (line[0] != '\n' && line[0] != '#')
                count++;

        if (count > 0)
            pcb->requests = (Request *)malloc(count * sizeof(Request));

        rewind(req_file);
        int idx = 0;
        while (fgets(line, sizeof(line), req_file))
        {
            if (line[0] == '\n' || line[0] == '#')
                continue;

            int time;
            unsigned int hex_addr;
            char rw_flag;

            if (sscanf(line, "%d %x %c", &time, &hex_addr, &rw_flag) == 3)
            {
                pcb->requests[idx].time = time;
                pcb->requests[idx].address = hex_addr;
                sprintf(pcb->requests[idx].binary_addr, "0x%02X", hex_addr);
                pcb->requests[idx].rwFlag = rw_flag;
                idx++;
            }
        }
        pcb->request_count = idx;
        fclose(req_file);
    }

    int pt_frame = find_free_frame();
    if (pt_frame == -1)
        pt_frame = nru_evict(NULL);

    pcb->PT_PhysicalAddress = pt_frame;

    RAM[pt_frame].is_free = false;
    RAM[pt_frame].is_PT = true;
    RAM[pt_frame].occupied_process = pcb->id;
    RAM[pt_frame].loaded_VPN = -1;
    RAM[pt_frame].referenced = 1;
    RAM[pt_frame].modified = 0;
    RAM[pt_frame].pt_limit = pcb->limit;

    RAM[pt_frame].page_table = (PTE *)malloc(pcb->limit * sizeof(PTE));
    for (int i = 0; i < pcb->limit; i++)
    {
        RAM[pt_frame].page_table[i].PhysicalAddress = -1;
        RAM[pt_frame].page_table[i].valid = false;
        RAM[pt_frame].page_table[i].refrenced = 0;
        RAM[pt_frame].page_table[i].modified = 0;
    }

    int first_page_frame = find_free_frame();
    if (first_page_frame == -1)
        first_page_frame = nru_evict(NULL);

    fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
            current_time, pcb->base + 0, pcb->id, first_page_frame);
    fflush(memory_log);

    RAM[first_page_frame].is_free = false;
    RAM[first_page_frame].is_PT = false;
    RAM[first_page_frame].occupied_process = pcb->id;
    RAM[first_page_frame].loaded_VPN = 0;
    RAM[first_page_frame].referenced = 1;
    RAM[first_page_frame].modified = 0;

    RAM[pt_frame].page_table[0].PhysicalAddress = first_page_frame;
    RAM[pt_frame].page_table[0].valid = true;

    pcb->last_request_idx = 0;
    pcb->reserved_frame = -1;
    pcb->pending_fault_vpn = -1;
}

int nru_evict(bool *is_modified)
{
    int victim = -1;
    int best_class = 4;

    for (int i = 0; i < 32; i++)
    {
        if (RAM[i].is_free || RAM[i].is_PT)
            continue;

        int r = RAM[i].referenced ? 1 : 0;
        int m = RAM[i].modified ? 1 : 0;
        int cls = r * 2 + m;

        if (cls < best_class)
        {
            best_class = cls;
            victim = i;
            if (cls == 0)
                break;
        }
    }

    if (victim == -1)
        return -1;

    int victim_proc = RAM[victim].occupied_process;
    int victim_vpn = RAM[victim].loaded_VPN;

    if (is_modified != NULL)
    {
        *is_modified = (RAM[victim].modified == 1) ? true : false;
    }

    if (RAM[victim].modified && memory_log)
    {
        fprintf(memory_log, "Swapping out page %d to disk\n", victim);
        fflush(memory_log);
    }

    if (victim_proc >= 0 && victim_vpn >= 0)
    {
        for (int j = 0; j < 32; j++)
        {
            if (RAM[j].is_PT && RAM[j].occupied_process == victim_proc)
            {
                if (RAM[j].page_table != NULL && victim_vpn < RAM[j].pt_limit)
                {
                    RAM[j].page_table[victim_vpn].valid = false;
                    RAM[j].page_table[victim_vpn].PhysicalAddress = -1;
                    RAM[j].page_table[victim_vpn].refrenced = 0;
                    RAM[j].page_table[victim_vpn].modified = 0;
                }
                break;
            }
        }
    }

    RAM[victim].is_free = false;
    RAM[victim].occupied_process = -1;
    RAM[victim].loaded_VPN = -1;
    RAM[victim].referenced = 0;
    RAM[victim].modified = 0;

    return victim;
}

int access_memory(PCB *pcb, int relative_time, int current_time)
{
    Request *current_req = NULL;
    for (int i = pcb->last_request_idx; i < pcb->request_count; i++)
    {
        if (pcb->requests[i].time == relative_time) 
        {
            current_req = &pcb->requests[i];
            pcb->last_request_idx = i + 1;
            break;
        }
    }

    if (current_req == NULL)
    {
        return NO_REQUEST;
    }

    int vpn = current_req->address / 16; 
    
    if(vpn>=pcb->limit) return NO_REQUEST;

    int pt_frame = pcb->PT_PhysicalAddress;
    PTE *my_pte = &RAM[pt_frame].page_table[vpn];

    if (my_pte->valid == true)
    {
        int physical_frame = my_pte->PhysicalAddress;

        my_pte->refrenced = 1;
        RAM[physical_frame].referenced = 1;

        if (current_req->rwFlag == 'w')
        {
            my_pte->modified = 1;
            RAM[physical_frame].modified = 1;
        }
        return MEMORY_HIT;
    }

    if (memory_log)
    {
        fprintf(memory_log, "PageFault upon VA %s from process %d\n",
                current_req->binary_addr, pcb->id);
        fflush(memory_log);
    }

    int target_frame = find_free_frame();
    int penalty = PAGE_FAULT_10;

    if (target_frame == -1)
    {
        bool victim_was_modified = false;
        target_frame = nru_evict(&victim_was_modified);

        if (victim_was_modified == true)
        {
            penalty = PAGE_FAULT_20;
        }
    }
    pcb->reserved_frame = target_frame;
    RAM[target_frame].is_free = false;
    pcb->pending_fault_vpn = vpn;

    return penalty;
}

void complete_page_fault(PCB *pcb, int current_time)
{
    int frame = pcb->reserved_frame;
    int vpn = pcb->pending_fault_vpn;

    if (frame == -1 || vpn == -1)
        return;

    RAM[frame].is_free = false;
    RAM[frame].is_PT = false;
    RAM[frame].occupied_process = pcb->id;
    RAM[frame].loaded_VPN = vpn;
    RAM[frame].referenced = 1;
    RAM[frame].modified = 0;

    int pt_frame = pcb->PT_PhysicalAddress;
    RAM[pt_frame].page_table[vpn].PhysicalAddress = frame;
    RAM[pt_frame].page_table[vpn].valid = true;
    RAM[pt_frame].page_table[vpn].refrenced = 0;
    RAM[pt_frame].page_table[vpn].modified = 0;

    int disk_address = pcb->base + vpn;
    if (memory_log)
    {
        fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
                current_time, disk_address, pcb->id, frame);
    }

    pcb->reserved_frame = -1;
    pcb->pending_fault_vpn = -1;
}

void clear_all_r_bits()
{
    for (int i = 0; i < 32; i++)
    {
        if (RAM[i].is_free)
            continue;

        if (RAM[i].is_PT == true)
        {
            int limit = RAM[i].pt_limit;
            for (int vpn = 0; vpn < limit; vpn++)
            {
                if (RAM[i].page_table[vpn].valid == true)
                {
                    RAM[i].page_table[vpn].refrenced = 0;
                }
            }
        }
        else
        {
            RAM[i].referenced = 0;
        }
    }

    printf("[MMU] Cleared all R-bits for NRU algorithm\n");
}

void free_process_memory(PCB *pcb)
{
    for (int i = 0; i < 32; i++)
    {
        if (RAM[i].occupied_process == pcb->id && !RAM[i].is_free)
        {
            if (RAM[i].is_PT && RAM[i].page_table != NULL)
            {
                free(RAM[i].page_table);
                RAM[i].page_table = NULL;
            }

            RAM[i].is_free = true;
            RAM[i].is_PT = false;
            RAM[i].occupied_process = -1;
            RAM[i].loaded_VPN = -1;
            RAM[i].referenced = 0;
            RAM[i].modified = 0;
        }
    }

    if (pcb->requests != NULL)
    {
        free(pcb->requests);
        pcb->requests = NULL;
    }

    printf("[MMU] Freed all memory for process %d\n", pcb->id);
}