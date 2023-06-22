
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include "vm_app.h"

using std::cout;
// lab 8 slide 58
int main() {
    /* 4 pages of physical memory in the system */
    if (fork()) { // parent
        char *page0 = (char *) vm_map(nullptr, 0);
        char *page1 = (char *) vm_map(nullptr, 0);
        char *page2 = (char *) vm_map(nullptr, 0);
        page0[0] = page1[0] = page2[0] = 'a';
    }
    else { // child
        char *page0 = (char *) vm_map(nullptr, 0);
        strcpy(page0, "Hello, world!");
    }
}