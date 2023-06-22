#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main()
{
    char  *filename = (char *) vm_map(nullptr, 0);
    strcpy(filename, "lampson83.txt");
    for (unsigned int i =0; i  < 1937; ++i){
        cout << filename[i];
    }
    char *p = (char *) vm_map(nullptr, 3);
    for (unsigned int i = 0; i  < 1937; ++i){
        cout << p[i];
    }
}