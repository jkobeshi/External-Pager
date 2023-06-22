#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

//evict something and bring it back
int main()
{
    char *filename0 = (char *) vm_map(nullptr, 0);
    strcpy(filename0, "lampson0.txt");
    
    char *filename1 = (char *) vm_map(nullptr, 0);
    strcpy(filename1, "lampson1.txt");

    char *filename2 = (char *) vm_map(nullptr, 0);
    strcpy(filename2, "lampson2.txt");

    char *filename3 = (char *) vm_map(nullptr, 0);
    
    //first evict
    strcpy(filename3, "lampson3.txt");
    
    //refresh filename1, evict filename2 in next clock
    strcpy(filename1, "lampson1.txt");

    char *filename4 = (char *) vm_map(nullptr, 0);
    strcpy(filename4, "lampson4.txt");

    char *filename5 = (char *) vm_map(nullptr, 0);
    strcpy(filename5, "lampson5.txt");

    //read
    strcpy(filename0, "new_string");
}