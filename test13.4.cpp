#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
// testing fork with swap backed
// fork from a process, parent creates swap backed page, writes to page, then dies, child wants to use that phys_mem spot
int main()
{

    
    if (fork()) { // parent
        char *p0 = (char *) vm_map(nullptr, 0);
        char *p1 = (char *) vm_map(nullptr, 0);
        char *p2 = (char *) vm_map(nullptr, 0);
        strcpy(p0, "lampson83.txt");
        strcpy(p1, "lampson83.txt");
        strcpy(p2, "lampson83.txt");
        
        char *p3 = (char *) vm_map(nullptr, 0);
        strcpy(p3, "lampson83.txt");

        char *p4 = (char *) vm_map(p1, 0);
        char *p5 = p4 + VM_PAGESIZE - 6;
        strcpy(p5, "lampson83.txt");
        for(int i = 0; i < 1937; ++i){
            cout << p1[i] << p5[i] << p4[i];
        }
    
    }
     else { // child
        char *p0 = (char *) vm_map(nullptr, 0);
        char *p1 = (char *) vm_map(nullptr, 0);
        char *p2 = (char *) vm_map(nullptr, 0);
        strcpy(p0, "lampson83.txt");
        strcpy(p1, "lampson83.txt");
        strcpy(p2, "lampson83.txt");
        
        char *p3 = (char *) vm_map(nullptr, 0);
        strcpy(p3, "lampson83.txt");

        char *p4 = (char *) vm_map(p1, 0);
        char *p5 = p4 + VM_PAGESIZE - 6;
        strcpy(p5, "lampson83.txt");
        for(int i = 0; i < 1937; ++i){
            cout << p1[i] << p5[i] << p4[i];
        }
    }
}