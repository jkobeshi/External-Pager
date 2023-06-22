#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

//tests for write on evict
int main()
{
    for(int i = 0; i < int(VM_ARENA_SIZE/VM_PAGESIZE);  ++i){
        char *page0 = (char *) vm_map(nullptr, 0);
        strcpy(page0, "lampson5.txt");
    }      
}