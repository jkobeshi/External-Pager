#include <iostream>
#include <cstring>
#include <unistd.h>
#include "vm_app.h"

using std::cout;

//evict something and bring it back
int main()
{
    if(fork()){
        char *p0 = (char *) vm_map(nullptr, 0);
        strcpy(p0, "lampson0.txt");
        char *p1 = (char *) vm_map(nullptr, 0);
        strcpy(p1, "lampson1.txt");
        char *p2 = p1 - 4;
        strcpy(p2, "GGGGGGGG");
        vm_yield();
        for(int i = 0; i < 1937; ++i){
            cout << p0[i];
        }
        for(int i = 0; i < 1937; ++i){
            cout << p1[i];
        }
    }
    else{
        char *p0 = (char *) vm_map(nullptr, 0);
        strcpy(p0, "3lampson.txt");
        char *p1 = (char *) vm_map(nullptr, 0);
        strcpy(p1, "4lampson.txt");
        char *p2 = p1 - 3;
        strcpy(p2, "FFFFFFFF");
        vm_yield();

        char *p3 = (char *) vm_map(nullptr, 0);
        strcpy(p3, "3lampson.txt");
        char *p4 = (char *) vm_map(nullptr, 0);
        strcpy(p4, "4lampson.txt");
        char *p5 = p3 - 3;
        strcpy(p5, "KKKKKKK");
        for(int i = 0; i < 1937; ++i){
            cout << p5[i];
        }

    }
}