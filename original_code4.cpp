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
    std::vector<bool> reference;
    std::vector<bool> resident;
    std::vector<bool> dirty;
    std::vector<bool> swap_back;
    std::vector<unsigned int> blk;
    std::vector<void*> file;
    int num_valid_page_entries;
};

int num_swap_back_pages; // must be <= max_swap_blocks
std::vector<bool> valid_in_phys; //valid bit for swap physmem
std::vector<bool> valid_in_swap; // valid bit for swap file

// hash map that stores pointers to processes based on the process id
std::unordered_map<pid_t, process*> process_map;

// deque used to determine eviction based on clock algorithm
std::deque<std::pair<process*, unsigned int>> eviction_queue;

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    max_phys_mem_pages = memory_pages;
    max_swap_blocks = swap_blocks;

    valid_in_phys.resize(max_phys_mem_pages, 0);
    valid_in_swap.resize(max_swap_blocks, 0);

    num_swap_back_pages = 0;
    num_processes = 0;

    valid_in_phys[0] = 1;
    for(unsigned int ind_next_avail = 0; ind_next_avail < VM_PAGESIZE; ++ind_next_avail){
        ((int*)vm_physmem)[ind_next_avail] = 0;
    }


}

int vm_create(pid_t parent_pid, pid_t child_pid){
    process* temp = new process();
    temp->arena = new page_table_t();
    temp->num_valid_page_entries = 0;
    temp->dirty.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->reference.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->resident.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->swap_back.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->file.resize(VM_ARENA_SIZE/VM_PAGESIZE);
    temp->blk.resize(VM_ARENA_SIZE/VM_PAGESIZE, -1);
    process_map[child_pid] = temp;
    num_processes++;
    return 0;
}

void *vm_map(const char *filename, unsigned int block){
    process* cur_process = process_map[current_pid];
    if(cur_process->num_valid_page_entries >= int(VM_ARENA_SIZE/VM_PAGESIZE)){
        return nullptr;
    } 
    if(filename == nullptr){ // swap backed page
        //eager swap reservation
        if(num_swap_back_pages >= int(max_swap_blocks)){
            return nullptr;
        }
        int swap_page = -1;
        for(int i = 0; i < (int)max_swap_blocks; ++i){
            if(valid_in_swap[i] == 0){
                swap_page = i;
                break;
            }
        }
        // initialize page table entry, point to zero page and enable read
        page_table_entry_t temp;
        temp.ppage = 0;
        temp.read_enable = 1;
        temp.write_enable = 0;
        cur_process->swap_back[cur_process->num_valid_page_entries] = 1;
        cur_process->arena->ptes[cur_process->num_valid_page_entries] = temp; // set page table entry to the initialized entry
        cur_process->blk[cur_process->num_valid_page_entries] = swap_page; // 
        valid_in_swap[swap_page] = 1;
        num_swap_back_pages++;
    }
    else{

        //point temp to ppage and write to it
        page_table_entry_t temp;
        
        temp.ppage = 0;
        temp.read_enable = 0;
        temp.write_enable = 0;
        cur_process->arena->ptes[cur_process->num_valid_page_entries] = temp;
        cur_process->blk[cur_process->num_valid_page_entries] = block;
        cur_process->file[cur_process->num_valid_page_entries] = (void*)filename;//address
        
    }
    cur_process->num_valid_page_entries++;
    return (void*)(intptr_t(VM_ARENA_BASEADDR) + ((cur_process->num_valid_page_entries - 1) * VM_PAGESIZE));
}

void vm_switch(pid_t pid){
    current_pid = pid;
    page_table_base_register = process_map[pid]->arena;
}

int vm_fault(const void* addr, bool write_flag){
    process* cur_process = process_map[current_pid];
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE); // get index of faulted address
    if((intptr_t(addr) < intptr_t(VM_ARENA_BASEADDR)) ||
        intptr_t(addr) >= (intptr_t(VM_ARENA_BASEADDR) + (intptr_t(VM_ARENA_SIZE) * cur_process->num_valid_page_entries))){ // check for out of arena bounds
            return -1;
    }
    
    // 3scenarios
    // in - change flag
    // not in has space - insert, change flag
    // not in no space - evict, insert, change flag
    if(cur_process->resident[addrIndex] == 0){
        //std::cout << "Not resident \n"; 
        //evict everytime resident = 0 except when ppage = 0 and write flag is false
        if(cur_process->arena->ptes[addrIndex].ppage != 0 || write_flag){ 
            
            
            int temp_ppage = max_phys_mem_pages;
            for(int i = 0; i < int(valid_in_phys.size()); ++i){
                if(valid_in_phys[i] == 0){ // find first 
                    temp_ppage = i;
                    break;
                } 
            }
            // eviction queue represents pages in phys_mem + 1 for zero page
            if(eviction_queue.size() + 1 >= max_phys_mem_pages) { // no space, need to evict
                int front_index = eviction_queue.front().second; // get index of page entry in process on clock hand
                while(eviction_queue.front().first->reference[front_index] == 1){ // loop until clock algo finds page entry to evict
                    //std::cout << "Skipping: " << eviction_queue.front().first->arena->ptes[front_index].ppage << "\n";
                    eviction_queue.front().first->reference[front_index] = 0;
                    //testing
                    eviction_queue.front().first->arena->ptes[front_index].read_enable = 0;
                    eviction_queue.front().first->arena->ptes[front_index].write_enable = 0;
                    //
                    eviction_queue.push_back(eviction_queue.front());
                    eviction_queue.pop_front();
                    front_index = eviction_queue.front().second;
                }
                // evict the page entry
                eviction_queue.front().first->resident[front_index] = 0;
                eviction_queue.front().first->arena->ptes[front_index].read_enable = 0;
                eviction_queue.front().first->arena->ptes[front_index].write_enable = 0;
                //std::cout << "EVICTING: " << eviction_queue.front().first->arena->ptes[front_index].ppage << "\n";
                if(eviction_queue.front().first->dirty[front_index] == 1){
                    uintptr_t buf = (uintptr_t)vm_physmem;
                    buf = buf + (uintptr_t(eviction_queue.front().first->arena->ptes[front_index].ppage) * uintptr_t(VM_PAGESIZE));
                    file_write(nullptr, eviction_queue.front().first->blk[front_index], (void*)buf);

                }
                eviction_queue.front().first->dirty[front_index] = 0;
                temp_ppage = eviction_queue.front().first->arena->ptes[front_index].ppage;
                valid_in_phys[temp_ppage] = 0;
                eviction_queue.pop_front();    
            } 
            valid_in_phys[temp_ppage] = 1;
            //cur_process->reference[addrIndex] = 1;
            cur_process->resident[addrIndex] = 1;


            //responsible for bringing evicted files back from swapfile
            if(cur_process->arena->ptes[addrIndex].ppage != 0){
                //std::cout << "HERE\n";
                uintptr_t buff = (uintptr_t)vm_physmem;
                buff = buff + (uintptr_t(cur_process->arena->ptes[addrIndex].ppage) * uintptr_t(VM_PAGESIZE));
                file_read(nullptr, cur_process->blk[addrIndex], (void*)buff);
            }

            cur_process->arena->ptes[addrIndex].ppage = temp_ppage;
            eviction_queue.push_back(std::pair(cur_process, addrIndex));
            if(write_flag && (cur_process->arena->ptes[addrIndex].ppage == 0)){
                for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
                    ((int*)vm_physmem)[(temp_ppage * VM_PAGESIZE) + i] = 0;
                }
            }
            
        }
        
        // TODO file-backed
        //std::cout << "addrIndex: " << addrIndex << '\n';
        
    }
    cur_process->reference[addrIndex] = 1;
    //std::cout << "reference set to 1: " << addrIndex << "\n";
    // page entry is now resident and valid, change flags
    if(write_flag){
        cur_process->dirty[addrIndex] = 1;
        cur_process->arena->ptes[addrIndex].write_enable = 1;
    }
    cur_process->arena->ptes[addrIndex].read_enable = 1;
    return 0;
}

void vm_destroy(){
    process* cur_process = process_map[current_pid];
    for(int i = 0; i < int(VM_ARENA_SIZE/VM_PAGESIZE); ++i){
        if(cur_process->swap_back[i] == 1){
            valid_in_swap[cur_process->blk[i]] = 0;
        }
        if(cur_process->resident[i] == 1){
            if(cur_process->arena->ptes[i].ppage != 0){
                valid_in_phys[cur_process->arena->ptes[i].ppage] = 0;
            }
            cur_process->resident[i] = 0;
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


