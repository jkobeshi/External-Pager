#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
//filename between two pages
int main()
{
    char *page0 = (char *) vm_map(nullptr, 0);
    char *page1 = (char *) vm_map(nullptr, 0);

    char *filename = page0 + VM_PAGESIZE - 3;
    strcpy(filename, "lampson83.txt");

    for (unsigned int i = 0; i  < 1937; ++i){
        cout << filename[i];
    }
    cout << "\n";
    char *page2 = (char *) vm_map(filename, 0);
    for (unsigned int i = 0; i  < 1937; ++i){
        cout << page2[i];
    }
}