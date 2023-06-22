#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
// testing fork with swap backed
// fork from a process, parent creates a page in phys_mem and yields, child evicts page and yields, parent reads from evicted page
int main()
{
    char *p0 = (char *) vm_map(nullptr, 0);
    char *p1 = (char *) vm_map(nullptr, 0);
    char *p2 = (char *) vm_map(nullptr, 0);
    strcpy(p0, "lampson83.txt");
    strcpy(p1, "lampson83.txt");
    strcpy(p2, "lampson83.txt");
    
    char *p3 = (char *) vm_map(nullptr, 0);
    strcpy(p3, "lampson83.txt");

    char *p4 = (char *) vm_map(p1, 0);

    
}