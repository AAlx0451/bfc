#include <stdio.h>
unsigned char tape[65536];
int ptr = 0;
int main() {
tape[ptr]++;
while(tape[ptr]) {
ptr++;
tape[ptr]++;
}
return 0;
}
