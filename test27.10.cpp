
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include "vm_app.h"

using std::cout;
int main() { /* 10 pages of physical memory in the system */
    char *filename = (char *) vm_map(nullptr, 0);
    strcpy(filename, "lampson83.txt");
    if (fork()) { // parent
        char *fb_page = (char *) vm_map(filename, 0);
        fb_page[0] = 'B';
        vm_yield();
    } else { // child
        char *fb_page = (char *) vm_map(filename, 0);
        assert(fb_page[0] == 'B');
        fb_page[0] = 'H';
    }
}