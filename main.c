#include <stdio.h>
#include <unistd.h>

int main() {

    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {                     //reads a byte, if there arent more bytes, ends


    }
    return 0;
}

