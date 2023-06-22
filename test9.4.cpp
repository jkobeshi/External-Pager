#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

int main()
{
    
    //filename
    char *p0 = (char *) vm_map(nullptr, 0);
    strcpy(p0, "lampson83.txt");

    
    char *p3 = (char *) vm_map (p0, 0);
    char *p1 = (char *) vm_map(nullptr, 0);
    char *p2 = (char *) vm_map(nullptr, 0);
    char *p5 = (char *) vm_map(nullptr, 0);
    char *p4 = (char *) vm_map(nullptr, 0);

    //tests write
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p3[i];
    }
    p1 = p1;
    p2 = p2;
    p4 = p4;
    p5 = p5;
}