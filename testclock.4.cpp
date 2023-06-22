#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main()
{   
    char *access;
    std::cout << "T1\n"; // 0
    /* Allocate swap-backed page from the arena */
    char *filename = (char *) vm_map(nullptr, 0);
    /* Write the name of the file that will be mapped */
    strcpy(filename, "lampson83.txt");
    
    std::cout << "T1\n"; // 1
    /* Allocate swap-backed page from the arena */
    access = (char *) vm_map(nullptr, 0);
    /* Write the name of the file that will be mapped */
    strcpy(access, "lampson83.txt");

    std::cout << "T1\n"; // 2
    /* Allocate swap-backed page from the arena */
    filename = (char *) vm_map(nullptr, 0);
    /* Write the name of the file that will be mapped */
    strcpy(filename, "lampson83.txt");

    std::cout << "T1\n"; // 3
    /* Allocate swap-backed page from the arena */
    filename = (char *) vm_map(nullptr, 0);
    /* Write the name of the file that will be mapped */
    strcpy(filename, "lampson83.txt");

    // access 2
    strcpy(access, "lampson83.txt");

    std::cout << "T1\n"; // 4
    /* Allocate swap-backed page from the arena */
    filename = (char *) vm_map(nullptr, 0);
    /* Write the name of the file that will be mapped */
    strcpy(filename, "lampson83.txt");
}