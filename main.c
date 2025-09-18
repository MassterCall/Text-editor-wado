#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>

    //original terminal attributes
    struct termios orig_termios;
    //disable raw mode
    void disableRawMode() {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        }
    //enable raw mode
    void enableRawMode() {
            //get current terminal attributes
            tcgetattr(STDIN_FILENO, &orig_termios );
            //disable raw mode at exit
            atexit(disableRawMode);
            //make a copy of the original attributes
            struct termios raw = orig_termios;
            //contains local flags || disable echoing and canonical mode
            raw.c_iflag &= ~(ICRNL | IXON);
            raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
            //contains input flags || disable ctrl-s and ctrl-q and CR to NL

            // set the terminal attributes
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        }





int main() {

    enableRawMode();
    char c;
    //reads a byte, if there aren't more bytes, ends if its q ends
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
            if (iscntrl(c)) {
                printf("%d\n", c);
            } else {
                printf("%d,('%c')\n", c,c);
            }
    }
    return 0;
}
