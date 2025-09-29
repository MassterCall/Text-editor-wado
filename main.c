/*** INCLUDES ***/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/
// Macro to simulate pressing Ctrl + a key.
#define CTRL_KEY(k) ((k) & 0x1f)
// Constant to initialize an empty append buffer.
#define ABUF_INIT {NULL, 0}
// Editor version.
#define WADO_VERSION "0.0.1"

// Enumeration for special keys to make the code more readable.
// Starts at 1000 to avoid collision with regular character ASCII values.
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_DOWN,
    PAGE_UP,
    HOME_KEY,
    END_KEY,
    DELETE_KEY
};

/*** DATA ***/
// store each row text
typedef struct textRow {
    int size;
    char *data;
} textRow;

// Structure to store the editor's global state.
struct editorConfig {
    int cx, cy; // Cursor X and Y coordinates.
    struct termios orig_termios; // Original terminal attributes.
    int screenrows; // Number of screen rows.
    int screencols; // Number of screen columns.
    int numrows;
    textRow *rows;
};

// Global instance of the configuration.
struct editorConfig config;

/*** TERMINAL FUNCTIONS ***/

// Function to handle fatal errors.
void die(const char *s) {
    // Clears the screen and positions the cursor at the top-left corner.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // Prints the system error message along with our custom message.
    perror(s);
    // Exits the program with a failure status.
    exit(1);
}

// Disables raw mode and restores the original terminal attributes.
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.orig_termios) == -1)
        die("tcsetattr");
}

// Enables raw mode for the terminal.
void enableRawMode() {
    // Saves the current terminal attributes.
    if (tcgetattr(STDIN_FILENO, &config.orig_termios) == -1) die("tcgetattr");
    // Registers disableRawMode to be called on program exit.
    atexit(disableRawMode);

    // Creates a copy of the attributes to modify them.
    struct termios raw = config.orig_termios;
    // Disables several input flags for full control.
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    // Disables output post-processing.
    raw.c_oflag &= ~(OPOST);
    // Sets the character size to 8 bits.
    raw.c_cflag |= (CS8);
    // Disables echo, canonical mode, and signals (Ctrl+C, Ctrl+Z).
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    // Sets a timeout for input reading.
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    // Applies the new attributes to the terminal.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

// Reads a key from the user, handling escape sequences for arrow keys.
int editorReadKey() {
    int nread;
    char c;
    // Waits until a byte is read or until the timeout expires.
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    // If the byte is an escape character, try to read the rest of the sequence.
    if (c == '\x1b') {
        char seq[3];
        // If the next two bytes can't be read, assume it was just the Esc key.
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        // If the sequence is '[A', '[B', etc., it translates it to our enum values.
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DELETE_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    } else {
        // Otherwise, it's a regular key.
        return c;
    }
}

// Gets the terminal window size.
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    // ioctl returns the window size in the ws struct.
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** FILE IN AND OUT ***/
// Appends a new row of text to the editor's rows.
void editorAppendRow(char *s, size_t len) {
    // Allocate memory for one more textRow
    config.rows = realloc(config.rows, sizeof(textRow) * (config.numrows + 1));
    if (config.rows == NULL) die("realloc");
    // Set the new row's size and data
    int ind = config.numrows;
    // Allocate memory for the row's data and copy the string into it
    config.rows[ind].size = len;
    config.rows[ind].data = malloc(len + 1);
    memcpy(config.rows[ind].data, s, len);
    // Null-terminate the string
    config.rows[ind].data[len] = '\0';
    config.numrows++;
}

// Opens a file and loads its content into the editor.
void editorOpen(char *nameFile) {
    FILE *fp = fopen(nameFile, "r");
    if (fp == NULL) die("fopen");

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    // FIX: Read each line from the file until getline returns -1
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        // Remove trailing newline or carriage return characters
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            linelen--;
        }
        // FIX: Append the processed line inside the loop
        editorAppendRow(line, linelen);
    }
    // FIX: Clean up memory used by getline and close the file
    free(line);
    fclose(fp);
}


/*** APPEND BUFFER ***/
// Structure for an append buffer (dynamic string).
struct abuf {
    char *b; // Pointer to the buffer in memory.
    int len; // Current length of the buffer.
};

// Appends a string to the append buffer, resizing memory if necessary.
void abAppend(struct abuf *ab, const char *s, int len) {
    // Requests a new memory block of the old size + the new size.
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    // Copies the new string to the end of the buffer.
    memcpy(&new[ab->len], s, len);
    // Updates the buffer's pointer and length.
    ab->b = new;
    ab->len += len;
}

// Frees the memory used by the append buffer.
void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** OUTPUT ***/

// Draws the status bar at the bottom of the screen.
void editorDrawStatusBar(struct abuf *ab) {
    // Activates inverted colors.
    abAppend(ab, "\x1b[7m", 4);

    char status[80];
    int len = snprintf(status, sizeof(status), " Wado Editor -- Version %s", WADO_VERSION);
    if (len > config.screencols) len = config.screencols;
    abAppend(ab, status, len);

    // Fills the rest of the bar with spaces.
    while (len < config.screencols) {
        abAppend(ab, " ", 1);
        len++;
    }
    // Deactivates inverted colors.
    abAppend(ab, "\x1b[m", 3);
}

// Draws the screen rows (file content or tildes).
void editorDrawRows(struct abuf *ab) {
    // Loop through all the rows available for the text area.
    for (int y = 0; y < config.screenrows - 1; y++) {
        // Check if the current screen row corresponds to a text row.
        if (y < config.numrows) {
            // If it does, draw the text.
            abAppend(ab, config.rows[y].data, config.rows[y].size);
        } else {
            // Otherwise, draw a tilde.
            abAppend(ab, "~", 1);
        }
        // Clears the rest of the line to avoid visual artifacts.
        abAppend(ab, "\x1b[K", 3);

        // Add a newline for every row to prepare for the status bar.
        abAppend(ab, "\r\n", 2);
    }
}

// Refreshes the entire screen.
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    // Hides the cursor, positions it at 1,1.
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    // Draws the main content and the status bar.
    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);

    // Moves the cursor to its logical position (cx, cy).
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", config.cy + 1, config.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    // Shows the cursor again.
    abAppend(&ab, "\x1b[?25h", 6);

    // Writes the entire buffer to the screen at once.
    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** INPUT ***/
// Updates the cursor coordinates (cx, cy) based on the key press.
void editorMoveCursor(int const key) {
    switch (key) {
        case PAGE_DOWN: config.cy = config.screenrows - 1;
            break;
        case PAGE_UP: config.cy = 0;
            break;
        case HOME_KEY: config.cx = 0;
            break;
        case END_KEY: config.cx = config.screencols - 1;
            break;
        case DELETE_KEY: break;
        case ARROW_LEFT:
            if (config.cx != 0) config.cx--;
            break;
        case ARROW_RIGHT:
            if (config.cx != config.screencols - 1) config.cx++;
            break;
        case ARROW_UP:
            if (config.cy != 0) config.cy--;
            break;
        case ARROW_DOWN:
            // FIX: Allow cursor to move to the line just after the last line of text.
            if (config.cy < config.screenrows - 1) config.cy++;
            break;
    }
}

// Processes the key pressed by the user.
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            // On exit, clear the screen.
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case PAGE_UP:
        case PAGE_DOWN:
        case HOME_KEY:
        case END_KEY:
        case DELETE_KEY:
            editorMoveCursor(c);
            break;
    }
}

/*** INIT ***/
// Initializes all fields of the global config structure.
void initEditor() {
    config.cx = 0;
    config.cy = 0;
    config.numrows = 0;
    config.rows = NULL;
    if (getWindowSize(&config.screenrows, &config.screencols) == -1)
        die("getWindowSize");

    // Reserves a row at the bottom for the status bar.
    config.screenrows--;
}

// Main program function.
int main(int argc, char *argv[]) {
    // Enables raw mode and initializes the editor.
    enableRawMode();
    initEditor();

    //Open the file specified in the command-line arguments, if any.
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    // Main program loop.
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
