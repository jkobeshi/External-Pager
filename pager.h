#ifndef _PAGER_H_
#define _PAGER_H_

#include "vm_pager.h"
#include <vector>
#include <deque>
#include <iostream>
#include <unordered_map>
#include <map>
#include <cstring>

struct process{
    page_table_t* arena;
    std::vector<bool> reference;
    std::vector<bool> resident;
    std::vector<bool> dirty;
    std::vector<bool> swap_back;
    std::vector<unsigned int> block;
    std::vector<char*> file;
    int num_pages;
};

class Pager {
public:
    Pager(unsigned int, unsigned int);
    ~Pager() { delete this; }
    
    void process_create(pid_t);
    void current_process_destroy();
    
    void create_page(const char*, unsigned int, unsigned int);
    unsigned int evict();

    int max_mem_pages;
    int max_swap_blocks;

    int num_sb_pages;

    pid_t current_pid;

    std::vector<bool> valid_in_phys;
    std::vector<bool> valid_in_swap;

    std::unordered_map<pid_t, process*> process_map;
    std::deque<std::pair<process*, unsigned int>> eviction_queue;

    std::map<std::pair<char*, unsigned int>, unsigned int> fb_exists; // stores ppage addr for file-backed 
    std::vector<std::vector<std::pair<process*, unsigned int>>> ppage_refs; // stores virtual pages
    //std::vector<int> ppage_refs_size;
};
#endif