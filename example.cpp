#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main()
{
    std::cout << "T1\n";
    /* Allocate swap-backed page from the arena */
    char *filename = (char *) vm_map(nullptr, 0);
    std::cout << "T2\n";
    /* Write the name of the file that will be mapped */
    strcpy(filename, "lampson83.txt");
    std::cout << "T3\n";
    /* Map a page from the specified file */
    char *p = (char *) vm_map (filename, 0);
    std::cout << "T4\n";
    /* Print the first part of the paper */
    for (unsigned int i=0; i<1937; i++) {
	    cout << p[i];
    }
}