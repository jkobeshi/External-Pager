#include "vm_pager.h"
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <cstring>

unsigned int max_phys_mem_pages; // maximum number of physical memory pages
unsigned int max_swap_blocks; // maximum number of swap blocks

pid_t current_pid; // current process id

int num_processes; // current number of processes that are alive


struct process{
    // stores information needed for a process
    page_table_t* arena;
    std::vector<bool> valid;
    std::vector<bool> reference;
    std::vector<bool> resident;
    std::vector<bool> dirty;
    std::vector<bool> swap_back;
    std::vector<unsigned int> sbpage; // swap backed page
    int num_valid_page_entries;
};

std::vector<page_table_entry_t> swap_file; // stores page table entries
int num_swap_back_pages; // must be <= max_swap_blocks
std::vector<bool> valid_in_swap; //valid bit for vm_physmem
std::vector<bool> valid_in_phys; //valid bit for swap_file

// hash map that stores pointers to processes based on the process id
std::unordered_map<pid_t, process*> process_map;

// deque used to determine eviction based on clock algorithm
std::deque<std::pair<process*, unsigned int>> eviction_queue;

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    /*
    Called when pager starts

    memory_pages = number of physical memory pages
    swap_blocks = number of swap blocks

    sets up data structures needed for vm_create
    creates zero page
    */
    max_phys_mem_pages = memory_pages;
    max_swap_blocks = swap_blocks;

    valid_in_phys.resize(max_phys_mem_pages, 0);
    swap_file.resize(swap_blocks);
    valid_in_swap.resize(swap_blocks, 0);

    num_swap_back_pages = 0;
    num_processes = 0;

    valid_in_phys[0] = 1;
    for(unsigned int ind_next_avail = 0; ind_next_avail < VM_PAGESIZE; ++ind_next_avail){
        ((int*)vm_physmem)[ind_next_avail] = 0;
    }
}

int vm_create(pid_t parent_pid, pid_t child_pid){
    /*
    called when parent process creates a new child process using fork

    parent_pid 
    initialize data structures needed to manage child proccess
    child arena set to be duplicate of parent arena (copy on write)
        child pages are set to have the same mapping as the corresponding page (both types)
        data in the child arena should be initialized to the parent's at the time of creation
    
    if parent process is not known, assume empty arena
    arenas will always be empty

    child process runs when switched to with vm_switch

    ensure there are enough available swap blocks (eager swap reservation)
        if can't return -1
        else return 0
    */

    // check if there is enough space for new process's swap blocks (eager swap reservation)
    // create a new process with empty arena and increment number of processes
    process* temp = new process();

    temp->arena = new page_table_t();
    temp->num_valid_page_entries = 0;
    temp->valid.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->dirty.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->reference.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->resident.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->swap_back.resize(VM_ARENA_SIZE/VM_PAGESIZE);
    temp->sbpage.resize(VM_ARENA_SIZE/VM_PAGESIZE, -1);

    process_map[child_pid] = temp;
    num_processes++;
    return 0;
}

void *vm_map(const char *filename, unsigned int block){
    /*
    called when a process wants to make another virtual page in its arena valid

    swap-backed pages
        filename is nullptr, ignore block
        new virtual page is swap-backed, private, initialized to 0
        stored in system swap file when no more free physical pages
        ensure there are enough swap blocks to hold all swap-backed virtual pages (eager swap reservation)
            if not, return nullptr

    file-backed pages
        filename is pointer to string in application address space that contains name of the file (access via phys mem)
        all virtual pages with the same filename and block are shared with each other
            all members in a set is represented as one node on clock queue
        return nullptr if filename not completely in valid part of arena
                        
    return address of new valid virtual page
    return nullptr if unable to handle request
    */
    process* cur_process = process_map[current_pid];
    if(cur_process->num_valid_page_entries >= int(VM_ARENA_SIZE/VM_PAGESIZE)){
        return nullptr;
    }
    
    if(filename == nullptr){ // swap backed page
        //eager swap reservation
        if(num_swap_back_pages >= int(max_swap_blocks)){
            return nullptr;
        }
        int sb_index = -1;
        for(int i = 0; i < int(max_swap_blocks); ++i){
            if(valid_in_swap[i] == 0){
                sb_index = i;
            }
        }
        // initialize page table entry, point to zero page and enable read
        page_table_entry_t temp;
        temp.ppage = 0;
        temp.read_enable = 1;
        temp.write_enable = 0;
        cur_process->swap_back[cur_process->num_valid_page_entries] = 1;
        cur_process->valid[cur_process->num_valid_page_entries] = 1; // set page to valid
        cur_process->arena->ptes[cur_process->num_valid_page_entries] = temp; // set page table entry to the initialized entry
        cur_process->sbpage[cur_process->num_valid_page_entries] = sb_index;
        valid_in_swap[num_swap_back_pages] = 1;
        cur_process->num_valid_page_entries++;
        num_swap_back_pages++;
    }
    else{
        //filebacked
    }
    return (void*)(intptr_t(VM_ARENA_BASEADDR) + ((cur_process->num_valid_page_entries - 1) * VM_PAGESIZE));
    
}

void vm_switch(pid_t pid){
    /*
    The infrastructure will switch processes for you. The pager's only job is to update the PTBR and pid
    */
    current_pid = pid;
    page_table_base_register = process_map[pid]->arena;
}

int vm_fault(const void* addr, bool write_flag){
    /*
    called when reading page that isnt read_enabled
    called when writing page that isnt write_enabled
    
    addr contains faulting address in app address space
    write_flag = true if access is write

    in memory (resident = 1), change flag
    not in memory not full, add into memory, change flag
    not in memory and full, evict, add into memory, change flag

    set flags so doesn't fault the next time

    return 0 on success
    return -1 if unable to handle (eg address invalid)
    */

    process* cur_process = process_map[current_pid];
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE); // get index of faulted address
    if((intptr_t(addr) < intptr_t(VM_ARENA_BASEADDR)) ||
        intptr_t(addr) > (intptr_t(VM_ARENA_BASEADDR) + intptr_t(VM_ARENA_SIZE))){ // check for out of arena bounds
            return -1;
    }
    
    if(cur_process->valid[addrIndex] == 0){ // return -1 if invalid
        return -1;
    }
    
    if(cur_process->resident[addrIndex] == 0){
        //evict everytime resident = 0 except when ppage = 0 and write flag is false
        if(cur_process->arena->ptes[addrIndex].ppage != 0 || write_flag){ 
            int temp_ppage = max_phys_mem_pages;
            for(int i = 0; i < int(valid_in_phys.size()); ++i){
                if(valid_in_phys[i] == 0){
                    temp_ppage = i;
                } 
            }
            // eviction queue represents pages in phys_mem + 1 for zero page
            if(eviction_queue.size() + 1 >= max_phys_mem_pages) { // no space, need to evict
                int front_index = eviction_queue.front().second; // get index of page entry in process on clock hand
                while(eviction_queue.front().first->reference[front_index] == 1){ // loop until clock algo finds page entry to evict
                    eviction_queue.front().first->reference[front_index] = 0;
                    eviction_queue.push_back(eviction_queue.front());
                    eviction_queue.pop_front();
                    front_index = eviction_queue.front().second;
                }
                // evict the page entry
                eviction_queue.front().first->resident[front_index] = 0;
                eviction_queue.front().first->arena->ptes[front_index].read_enable = 0;
                eviction_queue.front().first->arena->ptes[front_index].write_enable = 0;
                if(eviction_queue.front().first->dirty[front_index] == 1){
                    file_write(nullptr, )
                    swap_file[eviction_queue.front().first->sbpage[front_index]] =
                    eviction_queue.front().first->arena->ptes[front_index]; // make sure this is being set correctly
                }
                eviction_queue.front().first->dirty[front_index] = 0;
                temp_ppage = eviction_queue.front().first->arena->ptes[front_index].ppage;
                valid_in_phys[temp_ppage] = 0;
                eviction_queue.pop_front();    
            } 
            valid_in_phys[temp_ppage] = 1;
            cur_process->reference[addrIndex] = 1;
            cur_process->resident[addrIndex] = 1;
            cur_process->arena->ptes[addrIndex].ppage = temp_ppage;
            eviction_queue.push_back(std::pair(cur_process, addrIndex));
            if(write_flag && (cur_process->arena->ptes[addrIndex].ppage == 0)){
                for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
                    ((int*)vm_physmem)[(temp_ppage * VM_PAGESIZE) + i] = 0;
                }
            }
        }
        
    }
    // page entry is now resident and valid, change flags
    if(write_flag){
        cur_process->dirty[addrIndex] = 1;
        cur_process->arena->ptes[addrIndex].write_enable = 1;
    }
    else{
        cur_process->arena->ptes[addrIndex].read_enable = 1;
    }
    return 0;
}

void vm_destroy(){
    process* cur_process = process_map[current_pid];
    //make all swap backed pages in swap  file that are in this cur process invalid
    //valid_in_swap[cur_process->swap_file_base_index/VM_ARENA_SIZE/VM_PAGESIZE] = 0;
    for(int i = 0; i < int(VM_ARENA_SIZE/VM_PAGESIZE); ++i){
        if(cur_process->resident[i] == 1){
            if(cur_process->arena->ptes[i].ppage != 0){
                valid_in_phys[cur_process->arena->ptes[i].ppage] = 0;;
            }
            cur_process->resident[i] = 0;
        }
        else{
            valid_in_swap[cur_process->sbpage[i]] = 0;
        }
    } 
    unsigned int count = 0;
    while(count < eviction_queue.size()){
        if(eviction_queue.front().first->resident[eviction_queue.front().second] == 1){
            eviction_queue.push_back(eviction_queue.front());
        }
        eviction_queue.pop_front();
        ++count;
    }
    num_processes--;
    delete cur_process->arena;
    delete cur_process;
}


