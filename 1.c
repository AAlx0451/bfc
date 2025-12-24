#include <stdio.h>
#define TAPE 65536
unsigned char tape[TAPE];
unsigned char vstack[TAPE];
int pstack[TAPE];
int ptr = 0;
int vsp = 0;
int psp = 0;
int main() {
    tape[ptr] += 10;
    ptr += 1000;
    ptr -= 1;
    while(tape[ptr]) {
    tape[ptr] -= 1;
    }
    ptr += 1;
    ptr -= 1000;
    while(tape[ptr]) {
    ptr += 1000;
    ptr -= 1;
    tape[ptr] += 1;
    ptr += 1;
    ptr -= 1000;
    tape[ptr] -= 1;
    }
    ptr += 1000;
    while(tape[ptr]) {
    ptr += 2;
    }
    tape[ptr] += 1;
    ptr -= 2;
    while(tape[ptr]) {
    ptr += 1;
    while(tape[ptr]) {
    tape[ptr] -= 1;
    ptr += 2;
    tape[ptr] += 1;
    ptr -= 2;
    }
    ptr -= 3;
    }
    ptr += 1;
    while(tape[ptr]) {
    tape[ptr] -= 1;
    ptr += 2;
    tape[ptr] += 1;
    ptr -= 2;
    }
    ptr += 1;
    ptr -= 1000;
    /* PUSH */*/
    while(tape[ptr]) {
    tape[ptr] -= 1;
    }
    ptr += 1001;
    while(tape[ptr]) {
    ptr -= 1001;
    tape[ptr] += 1;
    ptr += 1001;
    tape[ptr] -= 1;
    }
    ptr += 1;
    while(tape[ptr]) {
    ptr += 1;
    while(tape[ptr]) {
    tape[ptr] -= 1;
    ptr -= 2;
    tape[ptr] += 1;
    ptr += 2;
    }
    ptr += 1;
    }
    ptr -= 2;
    while(tape[ptr]) {
    tape[ptr] -= 1;
    }
    ptr -= 2;
    while(tape[ptr]) {
    ptr -= 2;
    }
    ptr += 2;
    ptr -= 1000;
    /* POP */*/
    return 0;
}
