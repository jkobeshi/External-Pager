#include "vm_pager.h"
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <assert.h>

struct bits;

struct process{ 
    page_table_t* arena;
    std::vector<bits*> bit_fields;
    int num_pages;
};

struct bits{ // stores info on all bits 
    std::vector<std::pair<process*, unsigned int>> ptesBits;  // process pointer, index
    std::vector<bool> freed; //for destroy
    bool reference;
    bool resident;
    bool dirty;
    bool swap_back;
    unsigned int block;
    char* file;
    std::string filename;
};


int max_mem_pages;
int max_swap_blocks;
int num_sb_pages;
pid_t current_pid;

std::vector<bool> valid_in_phys; // valid_in_phys[5] checks if ppage 5 is valid
std::vector<bool> valid_in_swap; // valid_in_swap[5] checks if swap block 5 is valid

std::unordered_map<pid_t, process*> process_map;
std::map<std::pair<std::string, unsigned int>, bits*> fb_exists;
std::deque<bits*> eviction_queue;

void helperPrintPages(){
    // prints out info on all virtual pages
    std::cout << "\n Pager Helper Function: \n";
    process* value = process_map[current_pid];
    for(int i = 0; i < value->num_pages; ++i) {
        std::cout << "| vpage " << std::hex << (intptr_t)VM_ARENA_BASEADDR + (i * (intptr_t)VM_PAGESIZE) << " ";
        std::cout << "| ppage " << value->arena->ptes[i].ppage << " ";
        std::cout << "| read " << value->arena->ptes[i].read_enable << " ";
        std::cout << "| write " << value->arena->ptes[i].write_enable << " ";
        std::cout << "| swap backed? " << value->bit_fields[i]->swap_back << " ";
        std::cout << "| block " << value->bit_fields[i]->block << " ";
        std::cout << "| reference " << value->bit_fields[i]->reference << " ";
        std::cout << "| resident " << value->bit_fields[i]->resident << " ";
        std::cout << "| dirty " << value->bit_fields[i]->dirty << " ";
        std::cout << "|\n";
    }
}

void vm_destroy(){
    /*
    process* cur = process_map[current_pid];
    for(unsigned int i = 0; i < (unsigned int)cur->num_pages; ++i){ // iterate through pages
        //cur->bit_fields[i]->reference = 0;
        if(cur->bit_fields[i]->swap_back == 1){ // swap-backed - work to be done: set virtual and physical space to open 
            valid_in_swap[cur->bit_fields[i]->block] = 0; // free up space
            if(cur->bit_fields[i]->resident == 1){ // if resident, free up that space
                valid_in_phys[cur->arena->ptes[i].ppage] = 0;
            }
            if(cur->bit_fields[i]->freed == 0){ // old code
                cur->bit_fields[i]->freed = 1;
                delete cur->bit_fields[i];
            }
            num_sb_pages--; // manage for eager swap res
        }
        else{ // file-backed 
            if(cur->bit_fields[i]->resident == 1){ 
                if(cur->bit_fields[i]->ptesBits.size() == 1){
                    if(cur->bit_fields[i]->dirty == 1){ // don't do file write in this scenario, wait to evict
                        //uintptr_t buff = (uintptr_t)vm_physmem;
                        //buff = buff + (cur->arena->ptes[i].ppage * uintptr_t(VM_PAGESIZE));
                        //file_write((char*)cur->bit_fields[i]->filename.c_str(), cur->bit_fields[i]->block, (void*)buff);
                        //cur->bit_fields[i]->dirty = 0;
                        
                        
                    }
                }
                for(unsigned int j = 0; j < cur->bit_fields[j]->ptesBits.size(); ++j){ // remove current process from refs
                    if((cur->bit_fields[i]->ptesBits[j].first == cur) && (cur->bit_fields[i]->ptesBits[j].second == i)){
                        cur->bit_fields[i]->ptesBits[j] = cur->bit_fields[i]->ptesBits.back(); //copy back, pop to remove size
                        break;
                    }
                }               
                cur->bit_fields[i]->ptesBits.pop_back(); // popping back
                valid_in_phys[cur->arena->ptes[i].ppage] = 0;
            }
            else{
                if(cur->bit_fields[i]->freed == 0) { // old code
                    cur->bit_fields[i]->freed = 1;
                    delete cur->bit_fields[i];
                }
            }
        }
    }
    process_map.erase(current_pid);
    delete cur->arena;
    delete cur;
    */
}

int find_open_ppage(){
    /* 
     * returns ppage or -1 if no open ppage
     */
    int ppage = -1;
    for(int i = 0; i < int(valid_in_phys.size()); ++i){
        if(valid_in_phys[i] == 0){ 
            ppage = i;
            break;
        } 
    }
    return ppage;
}

std::string read_file_name(char* file, int cur_ind, bool write) {
    process* cur = process_map[current_pid];
    intptr_t fileIndex = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) / intptr_t(VM_PAGESIZE); //index of swap file we want to read from to get filename
    
    if(cur->bit_fields[fileIndex]->resident == 0){
        vm_fault(file, write);
    }
    intptr_t vm_start_byte = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) % intptr_t(VM_PAGESIZE);
    intptr_t cast = (intptr_t)vm_physmem;
    intptr_t phys_start_byte = cast + ((cur->arena->ptes[fileIndex].ppage) * (intptr_t)VM_PAGESIZE) + vm_start_byte;
    std::string temp_string = "";
    unsigned int tempIndex = fileIndex;

    while(((char*)phys_start_byte)[0] != '\0') {
        temp_string += ((char*)phys_start_byte)[0];
        phys_start_byte++;
        if(phys_start_byte >= ((cur->arena->ptes[tempIndex].ppage + 1) * (int)VM_PAGESIZE) + cast){
            tempIndex++;
            if(cur->bit_fields[tempIndex]->resident == 0){
                vm_fault((char*)((intptr_t)VM_PAGESIZE * tempIndex) + (intptr_t)VM_ARENA_BASEADDR, write);
            }
            phys_start_byte = cast + cur->arena->ptes[tempIndex].ppage * (intptr_t)VM_PAGESIZE;
        }      
    }
    return temp_string;
}

int evict_and_get_ppage(){
    assert(int(eviction_queue.size() + 1) >= max_mem_pages);

    while(eviction_queue.front()->reference == 1){ // exits when the front process has 0 as reference bit
        // update bits for every virtual page referencing the physical page at the front of the clock
        eviction_queue.front()->reference = 0;
        for(unsigned int i = 0; i < eviction_queue.front()->ptesBits.size(); ++i){
            if(eviction_queue.front()->freed[i] == 0){ // vpage already deleted, don't need to set
                eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].read_enable = 0;
                eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].write_enable = 0;
            }
        }
        // move clock hand
        eviction_queue.push_back(eviction_queue.front());
        eviction_queue.pop_front();
    }
    bits* evicted_field = eviction_queue.front();
    int open_ppage = find_open_ppage();
    
    eviction_queue.front()->resident = 0;
    valid_in_phys[open_ppage] = 0;
    eviction_queue.pop_front();

    if(evicted_field->dirty == 1) { // if trying to evict dirty page
        if(evicted_field->swap_back == 1){ // swapbacked, write to swapfile
            uintptr_t buf = (uintptr_t)vm_physmem;
            buf = buf + (open_ppage * uintptr_t(VM_PAGESIZE));
            int ret_value = file_write(nullptr, evicted_field->block, (void*)buf);
            if(ret_value == -1) {
                return -1;
            }
        }
        else{ // filebacked, write to file
            uintptr_t buff = (uintptr_t)vm_physmem;
            buff = buff + (open_ppage * uintptr_t(VM_PAGESIZE));
            int ret_value = file_write((char*)evicted_field->filename.c_str(), 
                evicted_field->block, (void*)buff);
            if(ret_value == -1) {
                return -1;
            }
        }
        evicted_field->dirty = 0;
    }
    /*
    std::vector<std::pair<process*, unsigned int>> temp;
    for(unsigned int i = 0; i < evicted_field->ptesBits.size(); ++i){
        if(evicted_field->freed[i] == 0){
            temp.push_back(evicted_field->ptesBits[i]);
        }
    }
    evicted_field->ptesBits = temp;
    if(evicted_field->ptesBits.empty()){
        delete evicted_field;
    }
    */
    return open_ppage;
}

int vm_fault_swap_backed(const void* addr, bool write_flag) {
    process* cur = process_map[current_pid];
    
    // addrIndex is the index into the page table
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE);  
    // if not already in phys_mem or trying to write to zero page, need to put a page into phys_mem
    if(cur->bit_fields[addrIndex]->resident == 0 && (write_flag || (cur->arena->ptes[addrIndex].ppage != 0))){
        int availPage = find_open_ppage();
        if(availPage == -1) {
            availPage = evict_and_get_ppage();
            if(availPage == -1){
                return -1;
            }
        }
        // put swapblock into phys_mem
        if(cur->arena->ptes[addrIndex].ppage == 0 && write_flag) {
            // dont need to file read, newly created block
            intptr_t first_byte = (intptr_t)vm_physmem + (intptr_t(availPage) * intptr_t(VM_PAGESIZE));
            memset((char*)first_byte, 0, VM_PAGESIZE);
        }
        else {
            // need to pull block from swapfile
            uintptr_t buff = (uintptr_t)vm_physmem;
            buff = buff + (uintptr_t(availPage) * uintptr_t(VM_PAGESIZE));
            int ret_value = file_read(nullptr, cur->bit_fields[addrIndex]->block, (void*)buff);
            if(ret_value == -1) { return -1; }
        }

        cur->arena->ptes[addrIndex].ppage = availPage;
        valid_in_phys[availPage] = 1;
        cur->bit_fields[addrIndex]->resident = 1;
        eviction_queue.push_back(cur->bit_fields[addrIndex]);
    }
    if(write_flag){
        cur->bit_fields[addrIndex]->dirty = 1;
        cur->arena->ptes[addrIndex].write_enable = 1;
    }
    cur->arena->ptes[addrIndex].read_enable = 1;
    return 0;
}

int vm_fault_file_backed(const void* addr, bool write_flag) {
    process* cur = process_map[current_pid];
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE);
    if(cur->bit_fields[addrIndex]->resident == 0){
        int availPage = find_open_ppage();
        if(availPage == -1) {
            availPage = evict_and_get_ppage();
            if(availPage == -1){
                return -1;
            }
        }
        
        cur->arena->ptes[addrIndex].ppage = availPage;
        valid_in_phys[availPage] = 1;
        cur->bit_fields[addrIndex]->resident = 1;
        eviction_queue.push_back(cur->bit_fields[addrIndex]);


        uintptr_t buff = (uintptr_t)vm_physmem;
        buff = buff + (availPage * uintptr_t(VM_PAGESIZE));
        int ret_value = file_read((char*)cur->bit_fields[addrIndex]->filename.c_str(), cur->bit_fields[addrIndex]->block, (void*)buff);
        if(ret_value == -1) {
            return -1;
        }

    }
    if(write_flag){
        cur->bit_fields[addrIndex]->dirty = 1;
        cur->arena->ptes[addrIndex].write_enable = 1;
    }
    cur->arena->ptes[addrIndex].read_enable = 1;
    return 0;
}

// addr is a virtual address
int vm_fault(const void* addr, bool write_flag){
    process* cur = process_map[current_pid];
    // check if address requested is valid in address space
    if((intptr_t(addr) < intptr_t(VM_ARENA_BASEADDR)) ||
        intptr_t(addr) >= (intptr_t(VM_ARENA_BASEADDR) + (intptr_t(VM_PAGESIZE) * cur->num_pages))){
            return -1;
    }
    // addrIndex is the index into the page table
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE);
    cur->bit_fields[addrIndex]->reference = 1; //? 
    if(cur->bit_fields[addrIndex]->swap_back == 1){
        return vm_fault_swap_backed(addr, write_flag);
    }
    else{
        return  vm_fault_file_backed(addr, write_flag);
    }
}

void *vm_map(const char *filename, unsigned int block){
    process* cur = process_map[current_pid];
    
    if(cur->num_pages >= int(VM_ARENA_SIZE/VM_PAGESIZE)){
        return nullptr;
    } 

    if(filename == nullptr){
        if(num_sb_pages >= max_swap_blocks){
            return nullptr;
        }
        int sf_block = 0; 
        for(int i = 0; i < max_swap_blocks; ++i){
            if(valid_in_swap[i] == 0){
                sf_block = i;
                break;
            }
        }
        cur->arena->ptes[cur->num_pages].ppage = 0;
        cur->arena->ptes[cur->num_pages].write_enable = 0;
        cur->arena->ptes[cur->num_pages].read_enable = 1;
        bits* temp_bits = new bits();
        temp_bits->resident = 0;
        temp_bits->reference = 0;
        temp_bits->swap_back = 1;
        temp_bits->dirty = 0;
        temp_bits->block = sf_block;
        temp_bits->ptesBits.push_back(std::pair(cur, cur->num_pages));
        temp_bits->freed.push_back(0);
        cur->bit_fields[cur->num_pages] = temp_bits;
        valid_in_swap[sf_block] = 1;
        ++num_sb_pages;
    }
    else{
        std::string file_str = read_file_name((char*)filename, cur->num_pages, 0);
        if(fb_exists.find(std::make_pair(file_str, block)) != fb_exists.end()){
            bits* fb_field = fb_exists[std::pair(file_str, block)];
            cur->arena->ptes[cur->num_pages].ppage = fb_field->ptesBits.front().first->arena->ptes[fb_field->ptesBits.front().second].ppage;
            cur->arena->ptes[cur->num_pages].read_enable = fb_field->ptesBits.front().first->arena->ptes[fb_field->ptesBits.front().second].read_enable;
            cur->arena->ptes[cur->num_pages].write_enable = fb_field->ptesBits.front().first->arena->ptes[fb_field->ptesBits.front().second].write_enable;
            fb_field->ptesBits.push_back(std::pair(cur, cur->num_pages));
            cur->bit_fields[cur->num_pages] = fb_field;
        }
        else{
            cur->arena->ptes[cur->num_pages].ppage = 0;
            cur->arena->ptes[cur->num_pages].read_enable = 0;
            cur->arena->ptes[cur->num_pages].write_enable = 0;
            bits* temp_bits = new bits();
            temp_bits->resident = 0;
            temp_bits->reference = 0;
            temp_bits->swap_back = 0;
            temp_bits->dirty = 0;
            temp_bits->block = block;
            temp_bits->ptesBits.push_back(std::pair(cur, cur->num_pages));
            temp_bits->freed.push_back(0);
            temp_bits->filename = file_str;
            cur->bit_fields[cur->num_pages] = temp_bits;
            fb_exists[std::pair(file_str, block)] = temp_bits;
        }
    }
    ++cur->num_pages;
    return (void*)(intptr_t(VM_ARENA_BASEADDR) + ((cur->num_pages - 1) * VM_PAGESIZE));
}

void vm_switch(pid_t pid){
    page_table_base_register = process_map[pid]->arena;
    current_pid = pid;
}

int vm_create(pid_t parent_pid, pid_t child_pid){
    // return 0 on success, -1 if no more swap space
    process* temp = new process();
    temp->arena = new page_table_t();
    temp->num_pages = 0;
    temp->bit_fields.resize(VM_ARENA_SIZE/VM_PAGESIZE);
    process_map[child_pid] = temp;
    return 0;
}

void vm_init(unsigned int memory_pages, unsigned int swap_blocks){
    // initialize pager

    // set global variables
    max_mem_pages = memory_pages;
    max_swap_blocks = swap_blocks;

    
    valid_in_phys.resize(max_mem_pages, 0);
    valid_in_swap.resize(max_swap_blocks, 0);

    // set zero page
    num_sb_pages = 0;
    valid_in_phys[0] = 1;
    for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
        ((char*)vm_physmem)[i] = 0;
    }
}



