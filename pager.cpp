#include "pager.h"
#include <assert.h>

Pager* pager;
bool debugging = true;

void helperPrintPages(){
    //pager->process_map->
    std::cout << "\n Pager Helper Function: \n";
    process* value = pager->process_map[pager->current_pid];
    for(int i = 0; i < value->num_pages; ++i) {
        std::cout << "| vpage " << std::hex << (intptr_t)VM_ARENA_BASEADDR + (i * (intptr_t)VM_PAGESIZE) << " ";
        std::cout << "| ppage " << value->arena->ptes[i].ppage << " ";
        std::cout << "| read " << value->arena->ptes[i].read_enable << " ";
        std::cout << "| write " << value->arena->ptes[i].write_enable << " ";
        std::cout << "| swap backed? " << value->swap_back[i] << " ";
        std::cout << "| block " << value->block[i] << " ";
        std::cout << "| reference " << value->reference[i] << " ";
        std::cout << "| resident " << value->resident[i] << " ";
        std::cout << "| dirty " << value->dirty[i] << " ";
        std::cout << "|\n";
    }
}
void vm_init(unsigned int memory_pages, unsigned int swap_blocks) {
    /*
     *  initialize pager with Pager object
     */
    pager = new Pager(memory_pages, swap_blocks);
}
int vm_create(pid_t parent_pid, pid_t child_pid) {
    /*
     *  run process_create using child pid in pager
     *  only fails due to eager swap reservation, not possible for core
     */
    pager->process_create(child_pid);
    return 0;
}
void vm_switch(pid_t pid) {
    /*
     *  changes PTBR and the current_pid to the pid we are switching to
     */
    page_table_base_register = pager->process_map[pid]->arena;
    pager->current_pid = pid;
}
void vm_destroy() {
    /*
     *  calls current_process_destroy on 
     */
    pager->current_process_destroy();
    
    if(debugging) {
        helperPrintPages();
    }
}
void *vm_map(const char *filename, unsigned int block) {
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
    
    process* cur = pager->process_map[pager->current_pid];
    if(cur->num_pages >= int(VM_ARENA_SIZE/VM_PAGESIZE)){ // already full
        return nullptr;
    } 
    if(filename == nullptr){  // swap-backed page
        if(pager->num_sb_pages >= pager->max_swap_blocks){ // if already full, unable to handle request
            return nullptr;
        }

        // find first available swap file block
        int sf_block = 0; 
        for(int i = 0; i < pager->max_swap_blocks; ++i){
            if(pager->valid_in_swap[i] == 0){
                sf_block = i;
                break;
            }
        }
        pager->create_page(filename,sf_block, 0);
    }
    else{ // file-backed page
        unsigned int phys_page = pager->max_mem_pages;
        // if the (filename, block) already exists then set phys_page to existing page
        if(pager->fb_exists.find(std::make_pair((char*)filename, block)) != pager->fb_exists.end()){
            phys_page = cur->arena->ptes[pager->fb_exists[std::make_pair((char*)filename, block)]].ppage;        
            pager->create_page(filename, block, phys_page);
        }
        // else if this pair does not exist then we want to evict and set it to the new ppage
        else{ // doesn't exist, we need to evict and 
            //phys_page = pager->evict(); // get open physical page
            pager->create_page(filename, block, phys_page);
            
            pager->fb_exists[std::make_pair((char*)filename, block)] = cur->num_pages - 1;
        }
    }
    if(debugging) {
        std::cout << "MAP";
        helperPrintPages();
    }
    return (void*)(intptr_t(VM_ARENA_BASEADDR) + ((cur->num_pages - 1) * VM_PAGESIZE));
}



int vm_fault(const void* addr, bool write_flag) {
    /*
    called when reading page that isnt read_enabled
    called when writing page that isnt write_enabled
    
    addr contains faulting address in app address space
    write_flag = true if access is write
    
    set flags so doesn't fault the next time

    return 0 on success
    return -1 if unable to handle (eg address invalid)
    */
    process* cur = pager->process_map[pager->current_pid];
    int addrIndex = ((intptr_t)addr - (intptr_t)VM_ARENA_BASEADDR) / intptr_t(VM_PAGESIZE);
    
    // check for outo
    if((intptr_t(addr) < intptr_t(VM_ARENA_BASEADDR)) ||
        intptr_t(addr) >= (intptr_t(VM_ARENA_BASEADDR) + (intptr_t(VM_PAGESIZE) * cur->num_pages))){
            return -1;
    }

    unsigned int oldPpage = cur->arena->ptes[addrIndex].ppage;
    if(cur->resident[addrIndex] == 0 && (write_flag || (oldPpage != 0))){
        unsigned int temp_ppage = pager->evict();
        cur->arena->ptes[addrIndex].ppage = temp_ppage;
        pager->valid_in_phys[temp_ppage] = 1;
        cur->resident[addrIndex] = 1; // SET TO RESIDENT
        helperPrintPages();
        if(cur->swap_back[addrIndex] == 1){ //only push back to queue here when swap-backed in made its own page or brought back 
            if(pager->ppage_refs[temp_ppage].empty()){
                pager->eviction_queue.push_back(std::make_pair(cur, addrIndex));    
                pager->ppage_refs[temp_ppage].push_back(std::make_pair(cur, addrIndex));
            }
        }
        else{
            if(pager->ppage_refs[temp_ppage].empty()){
                pager->eviction_queue.push_back(std::make_pair(cur, addrIndex));
            }
            pager->ppage_refs[temp_ppage].push_back(std::make_pair(cur, addrIndex));
        }
        assert(temp_ppage != 4);
        if(oldPpage != 0){
            //intptr_t phys_start_byte = 0;
            if(cur->swap_back[addrIndex] == 0){ //     
                char* file = cur->file[addrIndex];
                intptr_t fileIndex = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) / intptr_t(VM_PAGESIZE);
                if(cur->resident[fileIndex] == 0){
                    
                    vm_fault(file, 0);
                }
                intptr_t vm_start_byte = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) % intptr_t(VM_PAGESIZE);
                intptr_t cast = (intptr_t)vm_physmem;
                intptr_t phys_start_byte = cast + (cur->arena->ptes[fileIndex].ppage * (intptr_t)VM_PAGESIZE) + vm_start_byte;
                uintptr_t buff = (uintptr_t)vm_physmem;
                buff = buff + (temp_ppage * uintptr_t(VM_PAGESIZE));
                std::string temp_string = "";
                unsigned int tempIndex = fileIndex;
                while(((char*)phys_start_byte)[0] != '\0') {
                    temp_string += ((char*)phys_start_byte)[0];
                    std::cout << (char*)temp_string.c_str() << "\n";
                    phys_start_byte++;
                    if(phys_start_byte >= ((cur->arena->ptes[tempIndex].ppage + 1) * (int)VM_PAGESIZE) + cast) {
                        tempIndex++;
                        if(cur->resident[tempIndex] == 0){
                            vm_fault((char*)((intptr_t)VM_PAGESIZE * tempIndex) + (intptr_t)VM_ARENA_BASEADDR, 0);
                        }      
                        phys_start_byte = cast + cur->arena->ptes[tempIndex].ppage * (intptr_t)VM_PAGESIZE;
                        std::cout << "phys_start_byte r update: " << std::hex << (char*)phys_start_byte << '\n';

                    }
                }
                if(cur->resident[addrIndex] == 0) { //bring original file-backed back into physmem if it got evicted
                    vm_fault(addr, 0);
                }
                int ret_value = file_read((char*)temp_string.c_str(), cur->block[addrIndex], (void*)buff);
                if(ret_value == -1) {
                    return -1;
                }
            }
            else{
                uintptr_t buff = (uintptr_t)vm_physmem;
                buff = buff + (uintptr_t(temp_ppage) * uintptr_t(VM_PAGESIZE));
                int ret_value = file_read(nullptr, cur->block[addrIndex], (void*)buff);
                if(ret_value == -1) {
                    return -1;
                }
            } 
        }
        if(write_flag && (oldPpage == 0)){
            intptr_t first_byte = (intptr_t)vm_physmem + (intptr_t(temp_ppage) * intptr_t(VM_PAGESIZE));
            memset((char*)first_byte, 0, VM_PAGESIZE);
        }
    }
      
    for(unsigned int i = 0; i < pager->ppage_refs[cur->arena->ptes[addrIndex].ppage].size(); ++i){
        process* proc = pager->ppage_refs[cur->arena->ptes[addrIndex].ppage][i].first;
        unsigned int proc_ind = pager->ppage_refs[cur->arena->ptes[addrIndex].ppage][i].second; 
        proc->reference[proc_ind] = 1;
        if(write_flag){
            proc->dirty[proc_ind] = 1;
            proc->arena->ptes[proc_ind].write_enable = 1;
        }
        proc->arena->ptes[proc_ind].read_enable = 1;
    }
    if(debugging) {
        std::cout << "FAULT";
        helperPrintPages();
    }
    return 0;
}


void Pager::create_page(const char* filename, unsigned int block, unsigned int ppage){
    process* cur = pager->process_map[pager->current_pid];
    cur->arena->ptes[cur->num_pages].ppage = ppage;
    cur->block[cur->num_pages] = block;
    if(filename == nullptr){ // swap-backed
        cur->file[cur->num_pages] = (char*)filename;
        cur->arena->ptes[cur->num_pages].read_enable = 1;
        cur->arena->ptes[cur->num_pages].write_enable = 0;
        cur->swap_back[cur->num_pages] = 1;
        valid_in_swap[block] = 1;
        ++num_sb_pages;
    }
    else{ // file-backed    
        if(pager->fb_exists.find(std::make_pair((char*)filename, block)) == pager->fb_exists.end()) {
            cur->file[cur->num_pages] = (char*)filename;
            cur->arena->ptes[cur->num_pages].read_enable = 0;
            cur->arena->ptes[cur->num_pages].write_enable = 0;
            cur->reference[cur->num_pages] = 0;
            cur->dirty[cur->num_pages] = 0;
            cur->resident[cur->num_pages] = 0;
        }
        else{
            process* proc = pager->ppage_refs[ppage].front().first;
            unsigned int proc_ind = pager->ppage_refs[ppage].front().second;
            cur->file[cur->num_pages] = proc->file[proc_ind]; 
            cur->arena->ptes[cur->num_pages].read_enable = proc->arena->ptes[proc_ind].read_enable;
            cur->arena->ptes[cur->num_pages].write_enable = proc->arena->ptes[proc_ind].write_enable;
            cur->reference[cur->num_pages] = proc->reference[proc_ind];
            cur->dirty[cur->num_pages] = proc->dirty[proc_ind];
            cur->resident[cur->num_pages] = proc->resident[proc_ind]; // RESIDENT POSSIBLE
        }
        cur->swap_back[cur->num_pages] = 0;
    }
    ++cur->num_pages;
}


unsigned int Pager::evict(){
    /*
     *  returns open physical page slot, evicts if necessary
     */
    // find first empty physical page
    unsigned int temp_ppage = pager->max_mem_pages;
    for(int i = 0; i < int(valid_in_phys.size()); ++i){
        if(valid_in_phys[i] == 0){ 
            temp_ppage = i;
            break;
        } 
    }
    // evicts and overwrites temp_ppage if no space
    if(int(eviction_queue.size() + 1) >= pager->max_mem_pages) {
        int front_index = eviction_queue.front().second;
        process* front_process = eviction_queue.front().first;
    //evicting file backed page does not free up ppage
        // finds ppage to evict
        std::cout << "HERE1\n";
        while(front_process->reference[front_index] == 1){ // exits when the front process has 0 as reference bit
            // changes bits of every virtual page referencing the same physical page as the front of the the clock
            for(unsigned int i = 0; i < ppage_refs[front_process->arena->ptes[front_index].ppage].size(); ++i){
                process* proc = ppage_refs[front_process->arena->ptes[front_index].ppage][i].first;
                unsigned int proc_ind = ppage_refs[front_process->arena->ptes[front_index].ppage][i].second; 
                proc->reference[proc_ind] = 0;
                proc->arena->ptes[proc_ind].read_enable = 0;
                proc->arena->ptes[proc_ind].write_enable = 0;
            }
            // moves clock hand
            eviction_queue.push_back(eviction_queue.front());
            eviction_queue.pop_front();
            front_index = eviction_queue.front().second;
            front_process = eviction_queue.front().first;
        }
        eviction_queue.pop_front(); 
        valid_in_phys[front_process->arena->ptes[front_index].ppage] = 0;   
        if(front_process->dirty[front_index] == 1){    
            if(front_process->swap_back[front_index] == 0){
                char* file = front_process->file[front_index];
                intptr_t fileIndex = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) / intptr_t(VM_PAGESIZE);
                if(front_process->resident[fileIndex] == 0){
                    vm_fault(file, 0);
                }
                intptr_t vm_start_byte = (intptr_t(file) - intptr_t(VM_ARENA_BASEADDR)) % intptr_t(VM_PAGESIZE);
                intptr_t cast = (intptr_t)vm_physmem;
                intptr_t phys_start_byte = cast + (front_process->arena->ptes[fileIndex].ppage * (intptr_t)VM_PAGESIZE) + vm_start_byte;
                uintptr_t buff = (uintptr_t)vm_physmem;
                buff = buff + (front_process->arena->ptes[front_index].ppage * uintptr_t(VM_PAGESIZE));
                std::string temp_string = "";
                unsigned int tempIndex = fileIndex;
                while(((char*)phys_start_byte)[0] != '\0') {
                        if(front_process->resident[tempIndex] == 0){
                            vm_fault((char*)((intptr_t)VM_PAGESIZE * tempIndex) + (intptr_t)VM_ARENA_BASEADDR, 0);
                            phys_start_byte = cast + front_process->arena->ptes[tempIndex].ppage * (intptr_t)VM_PAGESIZE;
                            std::cout << "phys_start_byte w update: " << std::hex << (char*)phys_start_byte << '\n';
                        }      
                        temp_string += ((char*)phys_start_byte)[0];
                        std::cout << "temp_string: " << temp_string << '\n';
                        phys_start_byte++;
                        if(phys_start_byte >= ((front_process->arena->ptes[tempIndex].ppage + 1) * (int)VM_PAGESIZE) + cast) {
                            tempIndex++;
                            if(front_process->resident[tempIndex] == 1){
                                phys_start_byte = cast + front_process->arena->ptes[tempIndex].ppage * (intptr_t)VM_PAGESIZE;
                                std::cout << "phys_start_byte w update: " << std::hex << (char*)phys_start_byte << '\n';

                            }
                        }
                }
                if(front_process->resident[front_index] == 0) { //bring original file-backed back into physmem if it got evicted
                    vm_fault((char*)((intptr_t)VM_ARENA_BASEADDR + (front_index * (intptr_t)VM_PAGESIZE)), 1);
                }
                int ret_value = file_write((char*)temp_string.c_str(), front_process->block[front_index], (void*)buff);
                if(ret_value == -1) {
                    return -1;
                }
            }
            else {
                //swap backed
                uintptr_t buf = (uintptr_t)vm_physmem;
                buf = buf + (uintptr_t(front_process->arena->ptes[front_index].ppage) * uintptr_t(VM_PAGESIZE));
                int ret_value = file_write(nullptr, front_process->block[front_index], (void*)buf);
                if(ret_value == -1) {
                    return -1;
                }
            }
        }
        
        // changes bits of every virtual page referencing the same physical page as 
        temp_ppage = front_process->arena->ptes[front_index].ppage;
        valid_in_phys[temp_ppage] = 0;   
        std::cout << "EVICTION TEMP PPAGE: " << temp_ppage;
        for(unsigned int i = 0; i < ppage_refs[temp_ppage].size(); ++i){
            process* proc = ppage_refs[temp_ppage][i].first;
            unsigned int proc_ind = ppage_refs[temp_ppage][i].second; 
            proc->resident[proc_ind] = 0;
            proc->arena->ptes[proc_ind].read_enable = 0;
            proc->arena->ptes[proc_ind].write_enable = 0;
            proc->dirty[proc_ind] = 0;
        }
        ppage_refs[temp_ppage].clear();

    }
    //WE CALLED VM FAULT WITH  JUST EVICTING THEREFORE WE NEED TO KEEP VALID IN PHYS OUTSIDE
    //TO MAKE SURE THERE IS NOTHING THERE
    valid_in_phys[temp_ppage] = 0;
    //temp_ppage = 
    //assert(temp_ppage != 4);
    return temp_ppage;
}

Pager::Pager(unsigned int mem_pages, unsigned int swap_blocks){
    /*
     *  initializes the pager
     */
    max_mem_pages = mem_pages;
    max_swap_blocks = swap_blocks;

    valid_in_phys.resize(max_mem_pages, 0);
    valid_in_swap.resize(max_swap_blocks, 0);
    ppage_refs.resize(max_mem_pages);

    //std::cout << "V PHYSMEM: " << std::hex << (uintptr_t)vm_physmem;
    num_sb_pages = 0;
    valid_in_phys[0] = 1;
    for(unsigned int i = 0; i < VM_PAGESIZE; ++i){
        ((int*)vm_physmem)[i] = 0;
    }
    //memset(vm_physmem, 0, VM_PAGESIZE);
}

void Pager::process_create(pid_t pid){
    /*
     *  Creates empty process
     */
    process* temp = new process();
    temp->arena = new page_table_t();
    temp->num_pages = 0;
    temp->dirty.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->reference.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->resident.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->swap_back.resize(VM_ARENA_SIZE/VM_PAGESIZE, 0);
    temp->file.resize(VM_ARENA_SIZE/VM_PAGESIZE);
    temp->block.resize(VM_ARENA_SIZE/VM_PAGESIZE, -1);
    process_map[pid] = temp;
}

void Pager::current_process_destroy(){
    process* cur = process_map[current_pid];
    
    for(unsigned int i = 0; i < int(VM_ARENA_SIZE/VM_PAGESIZE); ++i){
        if(cur->swap_back[i] == 1){
            valid_in_swap[cur->block[i]] = 0;
            --num_sb_pages;
            if(cur->resident[i] == 1){
                if(cur->arena->ptes[i].ppage != 0){
                    valid_in_phys[cur->arena->ptes[i].ppage] = 0;
                    pager->ppage_refs[cur->arena->ptes[i].ppage].clear();
                }
                cur->resident[i] = 0;
                cur->reference[i] = 0;
                cur->dirty[i] = 0;
            }
        }
        else{
            if(cur->resident[i] == 1){
                valid_in_phys[cur->arena->ptes[i].ppage] = 0;
                for(unsigned int j = 0; j < pager->ppage_refs[cur->arena->ptes[i].ppage].size(); ++j){
                    if((pager->ppage_refs[cur->arena->ptes[i].ppage][j].first == cur) &&
                        (pager->ppage_refs[cur->arena->ptes[i].ppage][j].second == i)){
                        pager->ppage_refs[cur->arena->ptes[i].ppage][j] == pager->ppage_refs[cur->arena->ptes[i].ppage].back();
                        pager->ppage_refs[cur->arena->ptes[i].ppage].pop_back();
                        //pager->ppage_refs[cur->arena->ptes[i].ppage].shrink_to_fit();
                    }    
                }
                cur->resident[i] = 0;
                cur->reference[i] = 0;
                cur->dirty[i] = 0;
            }
        }
    }

    unsigned int i = 0;
    unsigned int eq_size = eviction_queue.size();
    while(i < eq_size){
        if(!pager->ppage_refs[eviction_queue.front().first->arena->ptes[eviction_queue.front().second].ppage].empty()){
            eviction_queue.push_back(eviction_queue.front());
        }
        eviction_queue.pop_front();
        ++i;
    }

    delete cur->arena;
    delete cur;
}