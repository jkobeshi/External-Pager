#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

//tests for evicting a swapfile, then bringing it back and writing again ,and evicting
int main()
{
    char *page0 = (char *) vm_map(nullptr, 0);
    char *page1 = (char *) vm_map(nullptr, 0);
    
    strcpy(page0, "lampson83.txt");

    if(fork()) {
        vm_yield();
    }
    else {
        page1[0] = 'a';
    }

    
        
    
}