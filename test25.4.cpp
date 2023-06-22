
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cassert>
#include "vm_app.h"

using std::cout;
// him
/*int main() {
    char *filename = (char *) vm_map(nullptr, 0);
    strcpy(filename, "lampson83.txt");

    char *fb_page = (char *) vm_map(filename, 0);
    fb_page[0] = 'B';
    
    fb_page[999999999999] = 'C';
}*/

int main() {
    char *filename = (char *) vm_map(nullptr, 0);
    strcpy(filename, "lampson83.txt");
    
    char *temp =  filename - VM_PAGESIZE + 1;
    char *null = (char *) vm_map(temp, 0);
    assert(null == nullptr);
}