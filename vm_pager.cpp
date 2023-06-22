#include "vm_pager.h"
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <assert.h>

bool debugging = 0;
struct bits;

struct process{ 
    page_table_t* arena;
    std::vector<bits*> bit_fields;
    std::vector<bool> deleted;
    int num_pages;
};

struct bits{ // stores info on all bits 
    std::vector<std::pair<process*, unsigned int>> ptesBits;  // process pointer, index
    unsigned int ppage;
    unsigned int read_enabled;
    unsigned int write_enabled;
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
        std::cout << "| read enabled " << value->bit_fields[i]->read_enabled << " ";
        std::cout << "| write enabled " << value->bit_fields[i]->write_enabled << " ";
        std::cout << "|\n";
    }
}

void vm_destroy() {
    process* cur = process_map[current_pid];
    for(unsigned int i = 0; i < (unsigned int)cur->num_pages; ++i){ // iterate through pages
        if(cur->bit_fields[i]->swap_back == 1){ // swap-backed - work to be done: set virtual and physical space to open,                               
                                                                                            //remove from eviction queue 
            valid_in_swap[cur->bit_fields[i]->block] = 0; // free up swapfile(virtual) space
            if(cur->bit_fields[i]->resident == 1){ // if resident, free up that physical space
                unsigned int evict_size = eviction_queue.size();
                for(unsigned int j = 0; j < evict_size; ++j){ // remove from eviction queue
                    if((eviction_queue.front()->ptesBits.front().first == cur) &&
                        (eviction_queue.front()->ptesBits.front().second == i)){
                        eviction_queue.pop_front();
                        
                    }
                    else{
                        eviction_queue.push_back(eviction_queue.front());
                        eviction_queue.pop_front();
                    }
                }
                valid_in_phys[cur->arena->ptes[i].ppage] = 0;
            }
            //always delete this bit field since sbfile and no other references.
            delete cur->bit_fields[i];
        }
        else{ // file-backed: work to be done - remove from refs
            if(cur->deleted[i] == 0){
                std::vector<std::pair<process*, unsigned int>> temp;
                for(unsigned int j = 0; j < cur->bit_fields[i]->ptesBits.size(); ++j){
                    if(cur->bit_fields[i]->ptesBits[j].first != cur){
                        temp.push_back(cur->bit_fields[i]->ptesBits[j]);
                    }
                    else{
                        //marks every index that this "bits" refers to as deleted so that
                        //when we dont do a double delete
                        cur->deleted[cur->bit_fields[i]->ptesBits[j].second] = 1;
                    }
                }
                cur->bit_fields[i]->ptesBits = temp;

                //when no other processes references this "bit field" and not resident,
                //we then we delete this bit field 
                if(cur->bit_fields[i]->ptesBits.empty() && (cur->bit_fields[i]->resident == 0)){
                    fb_exists.erase(std::pair(cur->bit_fields[i]->filename, cur->bit_fields[i]->block));
                    delete cur->bit_fields[i];
                }
            }
        }
    }
    process_map.erase(current_pid);
    delete cur->arena;
    delete cur;
    //if(debugging)
        //helperPrintPages();
}

int find_open_ppage(){
    //returns ppage or -1 if no open ppage
    int ppage = -1;
    for(int i = 0; i < int(valid_in_phys.size()); ++i){
        if(valid_in_phys[i] == 0){ 
            ppage = i;
            break;
        } 
    }
    return ppage;
}

std::string read_file_name(char* file, int cur_ind, bool write , int &test) {
    process* cur = process_map[current_pid];
    intptr_t fileIndex = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) / intptr_t(VM_PAGESIZE); //index of swap file we want to read from to get filename
    
    if(cur->bit_fields[fileIndex]->resident == 0){
        int fault = vm_fault(file, write);
        if(fault == -1){
            test = -1;
            return "";
        }
    }
    else{
        cur->bit_fields[fileIndex]->reference = 1;
        cur->bit_fields[fileIndex]->read_enabled = 1;
        if(cur->bit_fields[fileIndex]->dirty == 1){
            cur->bit_fields[fileIndex]->write_enabled = 1;
        }
        for(unsigned int i = 0; i < cur->bit_fields[fileIndex]->ptesBits.size(); ++i){
            cur->bit_fields[fileIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[fileIndex]->ptesBits[i].second].read_enable = 1;
            if(cur->bit_fields[fileIndex]->dirty == 1){
                cur->bit_fields[fileIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[fileIndex]->ptesBits[i].second].write_enable = 1;
            }
        }
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
                int fault = vm_fault((char*)((intptr_t)VM_PAGESIZE * tempIndex) + (intptr_t)VM_ARENA_BASEADDR, write);        
                if(fault == -1){
                    test = -1;
                    return "";
                }
            }
            else {
                //case of filename is resident but not read enabled
                cur->bit_fields[tempIndex]->reference = 1;
                cur->bit_fields[tempIndex]->read_enabled = 1;
                if(cur->bit_fields[tempIndex]->dirty == 1){
                    cur->bit_fields[tempIndex]->write_enabled = 1;
                }
                for(unsigned int i = 0; i < cur->bit_fields[tempIndex]->ptesBits.size(); ++i){
                    cur->bit_fields[tempIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[tempIndex]->ptesBits[i].second].read_enable = 1;
                    if(cur->bit_fields[tempIndex]->dirty == 1){
                        cur->bit_fields[tempIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[tempIndex]->ptesBits[i].second].write_enable = 1;
                    }
                }
            }
            phys_start_byte = cast + cur->arena->ptes[tempIndex].ppage * (intptr_t)VM_PAGESIZE;
        }      
    }
    return temp_string;
}

int evict_and_get_ppage(){
    
    while(eviction_queue.front()->reference == 1){ // exits when the front process has 0 as reference bit
        // update bits for every virtual page referencing the physical page at the front of the clock
        eviction_queue.front()->reference = 0;
        eviction_queue.front()->read_enabled = 0;
        eviction_queue.front()->write_enabled = 0;
        for(unsigned int i = 0; i < eviction_queue.front()->ptesBits.size(); ++i){
            eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].read_enable = 0;
            eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].write_enable = 0;
        }
        // move clock hand
        eviction_queue.push_back(eviction_queue.front());
        eviction_queue.pop_front();
    }
    
    //setting all evicted refs r/w bits to zero
    eviction_queue.front()->read_enabled = 0;
    eviction_queue.front()->write_enabled = 0;
    for(unsigned int i = 0; i < eviction_queue.front()->ptesBits.size(); ++i){
        eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].read_enable = 0;
        eviction_queue.front()->ptesBits[i].first->arena->ptes[eviction_queue.front()->ptesBits[i].second].write_enable = 0;
    }

    bits* evicted_field = eviction_queue.front();
    eviction_queue.pop_front();
    
    valid_in_phys[evicted_field->ppage] = 0;
    evicted_field->resident = 0;
    

    if(evicted_field->dirty == 1) { // if trying to evict dirty page
        uintptr_t buf = (uintptr_t)vm_physmem;
        buf = buf + (evicted_field->ppage * uintptr_t(VM_PAGESIZE));
        if(evicted_field->swap_back == 1){ // swapbacked, write to swapfile
            int ret_value = file_write(nullptr, evicted_field->block, (void*)buf);
            if(ret_value == -1) {
                return -1;
            }
        }
        else{ // filebacked, write to file
            int ret_value = file_write((char*)evicted_field->filename.c_str(), 
                evicted_field->block, (void*)buf);
            if(ret_value == -1) {
                return -1;
            }
    
        }
        evicted_field->dirty = 0;
    }
    
    if((evicted_field->swap_back == 0) && evicted_field->ptesBits.empty()){
        fb_exists.erase(std::pair(evicted_field->filename, evicted_field->block));    
        delete evicted_field;
    }
    
    return evicted_field->ppage;
}

int vm_fault_swap_backed(const void* addr, bool write_flag) {
    process* cur = process_map[current_pid];
    
    // addrIndex is the index into the page table
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE);  
    // if not already in phys_mem or trying to write to zero page, need to put a page into phys_mem
    if(cur->bit_fields[addrIndex]->resident == 0 && (write_flag || (cur->bit_fields[addrIndex]->ppage != 0))){
        int availPage = find_open_ppage();
        if(availPage == -1) {
            availPage = evict_and_get_ppage();
            if(availPage == -1){
                return availPage;
            }
        }
        // put swapblock into phys_mem
        if(cur->bit_fields[addrIndex]->ppage == 0 && write_flag) {
            // dont need to file read, newly created block
            uintptr_t buff = (uintptr_t)vm_physmem;
            intptr_t first_byte = (intptr_t)vm_physmem + (intptr_t(availPage) * intptr_t(VM_PAGESIZE));
            memcpy((char*)first_byte, (char*)buff, VM_PAGESIZE);
        }
        else {
            // need to pull block from swapfile
            uintptr_t buff = (uintptr_t)vm_physmem;
            buff = buff + (uintptr_t(availPage) * uintptr_t(VM_PAGESIZE));
            int ret_value = file_read(nullptr, cur->bit_fields[addrIndex]->block, (void*)buff);
            if(ret_value == -1) { return -1; }
        }

        cur->arena->ptes[addrIndex].ppage = availPage;
        cur->bit_fields[addrIndex]->ppage = availPage;
        valid_in_phys[availPage] = 1;
        cur->bit_fields[addrIndex]->resident = 1;
        eviction_queue.push_back(cur->bit_fields[addrIndex]);
    }
    if(write_flag || (cur->bit_fields[addrIndex]->dirty == 1)){
        cur->bit_fields[addrIndex]->dirty = 1;
        cur->bit_fields[addrIndex]->write_enabled = 1;
        cur->arena->ptes[addrIndex].write_enable = 1;
    }
    cur->bit_fields[addrIndex]->read_enabled = 1;
    cur->arena->ptes[addrIndex].read_enable = 1;
    if(debugging)
        helperPrintPages();
    
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
                return availPage;
            }
        }
        
        uintptr_t buff = (uintptr_t)vm_physmem;
        buff = buff + (availPage * uintptr_t(VM_PAGESIZE));
        int ret_value = file_read((char*)cur->bit_fields[addrIndex]->filename.c_str(), cur->bit_fields[addrIndex]->block, (void*)buff);
        if(ret_value == -1) {
            return -1;
        }


        cur->bit_fields[addrIndex]->ppage = availPage;
        for(unsigned int i = 0; i < cur->bit_fields[addrIndex]->ptesBits.size(); ++i){
            cur->bit_fields[addrIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[addrIndex]->ptesBits[i].second].ppage = availPage;
        }
        valid_in_phys[availPage] = 1;
        cur->bit_fields[addrIndex]->resident = 1;
        eviction_queue.push_back(cur->bit_fields[addrIndex]);

    }
    if(write_flag || (cur->bit_fields[addrIndex]->dirty == 1)){
        cur->bit_fields[addrIndex]->dirty = 1;
        cur->bit_fields[addrIndex]->write_enabled = 1;
        for(unsigned int i = 0; i < cur->bit_fields[addrIndex]->ptesBits.size(); ++i){
            cur->bit_fields[addrIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[addrIndex]->ptesBits[i].second].write_enable = 1;
        }
    }
    cur->bit_fields[addrIndex]->read_enabled = 1;
    for(unsigned int i = 0; i < cur->bit_fields[addrIndex]->ptesBits.size(); ++i){
        cur->bit_fields[addrIndex]->ptesBits[i].first->arena->ptes[cur->bit_fields[addrIndex]->ptesBits[i].second].read_enable = 1;
    }
    if(debugging)
        helperPrintPages();
    
    return 0;
}

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
    if(filename != nullptr) {

        if((intptr_t(filename) < intptr_t(VM_ARENA_BASEADDR)) ||
            intptr_t(filename) >= (intptr_t(VM_ARENA_BASEADDR) + (intptr_t(VM_PAGESIZE) * cur->num_pages))){
                return nullptr;
        }
    }
    if(cur->num_pages >= int(VM_ARENA_SIZE/VM_PAGESIZE)){
        return nullptr;
    } 

    if(filename == nullptr){
        int sf_block = -1; 
        for(int i = 0; i < max_swap_blocks; ++i){
            if(valid_in_swap[i] == 0){
                sf_block = i;
                break;
            }
        }
        if(sf_block == -1){ //eager swap res
            return nullptr;
        }

        cur->arena->ptes[cur->num_pages].ppage = 0;
        cur->arena->ptes[cur->num_pages].write_enable = 0;
        cur->arena->ptes[cur->num_pages].read_enable = 1;
        bits* temp_bits = new bits();
        temp_bits->ppage = 0;
        temp_bits->read_enabled = 1;
        temp_bits->write_enabled = 0;
        temp_bits->resident = 0;
        temp_bits->reference = 0;
        temp_bits->swap_back = 1;
        temp_bits->dirty = 0;
        temp_bits->block = sf_block;
        temp_bits->ptesBits.push_back(std::pair(cur, cur->num_pages));
        cur->bit_fields[cur->num_pages] = temp_bits;
        valid_in_swap[sf_block] = 1;
    }
    else{
        int test = 0;
        std::string file_str = read_file_name((char*)filename, cur->num_pages, 0, test);
        if(test == -1){
            return nullptr;
        }
        

        if(fb_exists.find(std::make_pair(file_str, block)) != fb_exists.end()){ // if we find already existing
            bits* fb_field = fb_exists[std::pair(file_str, block)];
            cur->arena->ptes[cur->num_pages].ppage = fb_field->ppage;
            cur->arena->ptes[cur->num_pages].read_enable = fb_field->read_enabled;
            cur->arena->ptes[cur->num_pages].write_enable = fb_field->write_enabled;
            fb_field->ptesBits.push_back(std::pair(cur, cur->num_pages));
            cur->bit_fields[cur->num_pages] = fb_field;
        }
        else{
            cur->arena->ptes[cur->num_pages].ppage = 0;
            cur->arena->ptes[cur->num_pages].read_enable = 0;
            cur->arena->ptes[cur->num_pages].write_enable = 0;
            bits* temp_bits = new bits();
            temp_bits->ppage = 0;
            temp_bits->read_enabled = 0;
            temp_bits->write_enabled = 0;
            temp_bits->resident = 0;
            temp_bits->reference = 0;
            temp_bits->swap_back = 0;
            temp_bits->dirty = 0;
            temp_bits->block = block;
            temp_bits->ptesBits.push_back(std::pair(cur, cur->num_pages));
            temp_bits->filename = file_str;
            cur->bit_fields[cur->num_pages] = temp_bits;
            fb_exists[std::pair(file_str, block)] = temp_bits;
            
        }
    }
    ++cur->num_pages;
    if(debugging)
        helperPrintPages();
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
    temp->deleted.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
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
    valid_in_phys[0] = 1;
    for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
        ((char*)vm_physmem)[i] = 0;
    }
}



