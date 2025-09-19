/*** INCLUDES ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k)& 0x1f)


/*** DATA ***/

// Stores the original terminal attributes to restore them on exit.
struct termios orig_termios;

/*** TERMINAL FUNCTIONS ***/

void die(const char *s) {
    // Prints a descriptive error message and exits the program with a failure status.
    perror(s);
    exit(1);
}

void disableRawMode() {
    // Restores the original terminal attributes.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    // Gets the current terminal attributes.
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    // Schedules disableRawMode to be called automatically on program exit.
    atexit(disableRawMode);

    // Creates a copy of the original attributes to modify.
    struct termios raw = orig_termios;

    // c_iflag: Modifies input flags.
    // Disables break signal, parity check, stripping 8th bit, Ctrl-S/Q, and CR-to-NL.
    raw.c_iflag &= ~(BRKINT | INPCK | ISTRIP | ICRNL | IXON);

    // c_oflag: Modifies output flags.
    // Disables all output post-processing (e.g., converting '\n' to '\r\n').
    raw.c_oflag &= ~(OPOST);

    // c_cflag: Modifies control flags.
    // Sets the character size to 8 bits per byte.
    raw.c_cflag |= (CS8);

    // Disables echo, canonical mode, signals (Ctrl-C, Ctrl-Z), and Ctrl-V.
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    // c_cc: Sets control characters (timeout configuration).
    // VMIN = 0: read() returns as soon as there is any data to read.
    raw.c_cc[VMIN] = 0;
    // VTIME = 1: read() will wait for 100ms before timing out.
    raw.c_cc[VTIME] = 1;

    // Applies the modified attributes to the terminal.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorKeyRead() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

void editorProcessKeypress() {
    char const c = editorKeyRead();
    if (c == CTRL_KEY('w')) exit(0);
}

/*** INIT ***/

int main() {
    enableRawMode();

    // Main loop to read and process input.
    while (1) {
      editorProcessKeypress();
    return 0;
};
}
