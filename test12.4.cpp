#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
//testing multiple virtual pages pointing to one file-backed page
int main(){
    char *page1 = (char *) vm_map(nullptr, 0); //0
    char *page2 = (char *) vm_map(nullptr, 0); //1
    
    char *filename = page1 + VM_PAGESIZE - 4;
    strcpy(filename, "lampson83.txt");

    std::cout << "\n\nRUNNING LAMPSON\n\n";
    char *p = (char *) vm_map (filename, 0); //2
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p[i];
    }

    std::cout << "\n\nRUNNING LAMPSON2\n\n";
    char *p1 = (char *) vm_map(filename, 0); //3
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p1[i];
    }

    std::cout << "\n\nRUNNING LAMPSON3\n\n";
    char *p2 = (char *) vm_map(filename, 0); //4
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p2[i];
    }

    char *p3 = (char *) vm_map(nullptr, 0); //5
    strcpy(p3, "data1.bin");

    char *p4 = (char *) vm_map(nullptr, 0); //6
    strcpy(p4, "Dwaf");

    std::cout << "\n\nCOUTING DWAF\n\n";
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p4[i];
    }

    char *p31 = (char *) vm_map(nullptr, 0); //7
    strcpy(p31, "dwad");

    char *p41 = (char *) vm_map(nullptr, 0); //8
    strcpy(p41, "Dwaf");

    char *p51 = (char *) vm_map(nullptr, 0); //9
    strcpy(p51, "dwad");

    char *p61 = (char *) vm_map(nullptr, 0); //a
    strcpy(p61, "Dwaf");

    std::cout << "\n\nRUNNING LAMPSON4\n\n";
    char *p7 = (char *) vm_map(filename, 0); //b
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p7[i];
    }
    std::cout << "\n\nRUNNING Data1.bin\n\n";
    char *p8 = (char *) vm_map(p3, 0); //c
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p8[i];
    }
    std::cout << "\n\nRUNNING LAMPSON5\n\n";
    for (unsigned int i=0; i< 1937; i++) {
	    cout << p1[i];
    }

    std::cout << "\n\nRUNNING Data1.bin2\n\n";
    p2 = (char*) vm_map(p3, 0); //d
    for(unsigned int i = 0; i < 1937; ++i) {
        cout << p2[i];
    }
}
