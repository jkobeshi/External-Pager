#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;
// testing fork with swap backed

int main()
{
    char *original1 = (char *) vm_map(nullptr, 0);
    strcpy(original1, "lampson83.txt");

    char *original2 = (char *) vm_map(nullptr, 0);
    strcpy(original2, "lampson83.txt");

    char *original3 = (char *) vm_map(nullptr, 0);
    strcpy(original3, "lampson83.txt");

    char *original4 = (char *) vm_map(nullptr, 0);
    strcpy(original4, "lampson83.txt");
    

    if (fork()) { // parent
        strcpy(original2, "reset reference bit");

        char *parent1 = (char *) vm_map(nullptr, 0);
        strcpy(parent1, "lampson83.txt");

        for (unsigned int i=0; i< 1937; i++) {
            cout << parent1[i];
        }
        vm_yield(); // yield to child


        char *parent2 = (char *) vm_map(nullptr, 0);
        strcpy(parent2, "lampson83.txt");
    }
    else { // child
        char *child1 = (char *) vm_map(nullptr, 0);
        char *child2 = (char *) vm_map(nullptr, 0);
        char *child3 = (char *) vm_map(nullptr, 0);

        strcpy(child1, "lampson83.txt");
        strcpy(child2, "data1.bin");
        strcpy(child3, "data2.bin");


        char *child4 = (char *) vm_map(child1, 0);
        for (unsigned int i=0; i< 1937; i++) {
            cout << child4[i];
        }
        for (unsigned int i=0; i< 1936; i++) {
            char * temp = original2 + i;
            strcpy(temp, "a");
        }

        vm_yield();
        for (unsigned int i=0; i< 1937; i++) {
            cout << original1[i];
        }
    }
}