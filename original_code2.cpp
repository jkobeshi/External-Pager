#include "vm_pager.h"
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <cstring>

unsigned int max_mem_pages; // number of physical memory pages
unsigned int max_swap_blocks; // number of swap blocks
int cur_num_mem_pages; // current number of physical memory pages
int cur_num_swap_blocks;
pid_t current_pid;
int num_processes;
struct process { //might change from struct to a mapping from pid to page table
    page_table_t* process_page_table;
    bool reference[VM_ARENA_SIZE/VM_PAGESIZE];
    bool resident[VM_ARENA_SIZE/VM_PAGESIZE];
    bool dirty[VM_ARENA_SIZE/VM_PAGESIZE];
    bool swapback[VM_ARENA_SIZE/VM_PAGESIZE];
    bool valid[VM_ARENA_SIZE/VM_PAGESIZE];
    unsigned int block[VM_ARENA_SIZE/VM_PAGESIZE];
    int swap_file_page[VM_ARENA_SIZE/VM_PAGESIZE];
    unsigned int vm_arena_cnt;
    unsigned int proc_num_swap_blocks;
};

std::unordered_map<pid_t, process*> process_map; // stores pid and process page table base register
std::deque<std::pair<process*, unsigned int>> eviction_queue;
page_table_t swap_file; 

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    /*
    Called when pager starts

    memory_pages = number of physical memory pages
    swap_blocks = number of swap blocks

    sets up data structures needed for vm_create
    creates zero page
    */
    max_mem_pages = memory_pages;
    max_swap_blocks = swap_blocks;
    cur_num_mem_pages = 0;
    cur_num_swap_blocks = 0;
    num_processes = 0;
    for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
        ((int*)vm_physmem)[i] = 0;
    }
    cur_num_mem_pages++;
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
    
    //check if there is enough space for swap block - global eager swap reservation
    if ((num_processes * (VM_ARENA_SIZE/VM_PAGESIZE)) >= max_swap_blocks){
        return -1;
    }

    process* temp = new process();
    temp->process_page_table = new page_table_t();
    temp->vm_arena_cnt = 0;
    temp->proc_num_swap_blocks = 0;

    //set mapping for swap fil
    for(int i = 0; i < int(VM_ARENA_SIZE/VM_PAGESIZE); ++i){
        temp->swap_file_page[i] = cur_num_swap_blocks;
        temp->valid[i] = 0;
        cur_num_swap_blocks++;
    }
    process_map[child_pid] = temp;
    num_processes++;
    return 0;  
}

//switches process by setting PTBR to the current process's page table.
void vm_switch(pid_t pid){
    /*
    The infrastructure will switch processes for you. The pager's only job is to update the PTBR and pid
    */
    page_table_base_register = process_map[pid]->process_page_table;
    current_pid = pid;
}

int vm_fault(const void* addr, bool write_flag){
    /*
    called when reading page that isnt read_enabled
    called when writing page that isnt write_enabled
    
    addr contains faulting address in app address space
    write_flag = true if access is write
    
    set flags so doesn't fault the next time

    return 0 on success
    return -1 if unable to handle (eg address invalid)
    */
    //clock algorithm
    //if addr is invalid: return -1
    
    // in memory (resident = 1), change flag
    // not in memory not full, add into memory, change flag
    // not in memory and full, evict, add into memory, change flag

    // checks if addr is in address space
    process* cur_process = process_map[current_pid];
    // get index of addr
    int index = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / VM_PAGESIZE;
    if(cur_process->valid[index] == 0){
        return -1;
    }

    //std::deque<std::pair<process*, unsigned int>> eviction_queue;

    //eviction block
    //first condition - don't evict for reads on zero page
    //second condtion - write to ppage
    if((cur_process->resident[index] == 0) && 
        ((cur_process->process_page_table->ptes[index].ppage != 0) ||
        (write_flag && (cur_process->process_page_table->ptes[index].ppage == 0)))){
        unsigned int temp_ppage = cur_num_mem_pages;
        // evict if full - clock algo
        if(cur_num_mem_pages >= int(max_mem_pages)){
            int ref_index = eviction_queue.front().second;
            while(eviction_queue.front().first->reference[ref_index] == 1){
                eviction_queue.front().first->reference[ref_index] = 0;
                eviction_queue.push_back(eviction_queue.front());
                eviction_queue.pop_front();
                // set after pop
                ref_index = eviction_queue.front().second;
            }
            // exits when we find something to evict
            // TODO check dirty bit for file backed 
            eviction_queue.front().first->resident[ref_index] = 0;
            temp_ppage = eviction_queue.front().first->process_page_table->ptes[ref_index].ppage;
            //when we evict do we have to set read and write flag back to 0;
            eviction_queue.pop_front(); 
            --cur_num_mem_pages;
        }
        //add into phys mem
        cur_process->process_page_table->ptes[index].ppage = temp_ppage;
        swap_file.ptes[cur_process->swap_file_page[index]].ppage = temp_ppage;
        cur_process->resident[index] = 1;
        cur_process->resident[index] = 1;
        if(write_flag && (cur_process->process_page_table->ptes[index].ppage == 0)){
            for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
                ((int*)vm_physmem)[(temp_ppage * VM_PAGESIZE) + i] = 0;
            }
        }
        eviction_queue.push_back(std::pair(cur_process, index));
        ++cur_num_mem_pages;
    }

    // set correct flag
    if (write_flag) { // write fault
        cur_process->dirty[index] = 1;
        cur_process->process_page_table->ptes[index].write_enable = 1;
    }
    else { // read fault
        file_read((char*)addr, cur_process->block[index], 
            (void*)((char*)vm_physmem)[cur_process->process_page_table->ptes[index].ppage * intptr_t(VM_PAGESIZE)]);
        cur_process->process_page_table->ptes[index].read_enable = 1;
    }
    return 0;
}

void vm_destroy(){
    for(auto &it : process_map){
        delete it.second->process_page_table;
        delete it.second;
    }
}

/*
Swap-backed pages are stored in the system's swap file when there are no free physical pages.
vm_map should ensure that there are enough swap blocks to hold all 
swap-backed virtual pages (eager swap reservation), otherwise vm_map should return nullptr.
*/
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
    //checks for enough space in arena
    if((cur_process->vm_arena_cnt * VM_PAGESIZE) >= VM_ARENA_SIZE){
        return nullptr;
    }

    if (filename == nullptr){
        //initialize swap back file

        // creates temp page
        page_table_entry_t temp;
        temp.ppage = 0;
        temp.read_enable = 1;
        temp.write_enable = 0;
        //ensures we map to lowest count
        while(cur_process->valid[cur_process->vm_arena_cnt] == 1){
            ++cur_process->vm_arena_cnt;
        }
        cur_process->swapback[cur_process->vm_arena_cnt] = 1;
        cur_process->dirty[cur_process->vm_arena_cnt] = 0;
        cur_process->process_page_table->ptes[cur_process->vm_arena_cnt] = temp;
        cur_process->resident[cur_process->vm_arena_cnt] = 0;
        cur_process->reference[cur_process->vm_arena_cnt] = 0;
        cur_process->valid[cur_process->vm_arena_cnt] = 1;
        // if physmem full but swapfil s
        if(cur_process->proc_num_swap_blocks < VM_ARENA_SIZE/VM_PAGESIZE){
            swap_file.ptes[cur_process->swap_file_page[cur_process->vm_arena_cnt]] = temp;
            cur_process->proc_num_swap_blocks++;
            cur_process->vm_arena_cnt++;
            return (void*)(intptr_t(VM_ARENA_BASEADDR) + ((cur_process->vm_arena_cnt - 1) * VM_PAGESIZE));   

        }
        else{
            return nullptr;
        }
        //put into physmem if physmem isnt full
        //put into swap file if physmem is full and swapfile isnt full
        //return nullptr if both physmem and swapfile is full
    }
    else{
        if ((intptr_t(filename) < intptr_t(VM_ARENA_BASEADDR)) ||  
            (intptr_t(filename) > (intptr_t(VM_ARENA_BASEADDR) + intptr_t(VM_ARENA_SIZE)))){
                return nullptr;
        }
        int index = ((intptr_t)filename - (intptr_t)VM_ARENA_BASEADDR) / VM_PAGESIZE;
        cur_process->swapback[index] = 0;
        cur_process->dirty[index] = 0;
        cur_process->process_page_table->ptes[index].ppage = cur_num_mem_pages;
        cur_num_mem_pages++;
        cur_process->process_page_table->ptes[index].read_enable = 0;
        cur_process->process_page_table->ptes[index].write_enable = 0;
        cur_process->resident[index] = 1;
        cur_process->reference[index] = 1;
        cur_process->valid[index] = 1;
        cur_process->block[index] = block;
        return (void*)filename;   

        //fileback
    }
    
}
