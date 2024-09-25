#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#define KILO_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};
struct editorConfig {
    int screenRows;
    int screenCols;
    struct termios orig_termios;
    int cx, cy;
};
struct editorConfig E;

void cleanScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4); //The 4 means we are writing 4 bytes out to the terminal. The first byte is \x1b, which is the escape character, or 27 in decimal.
    write(STDOUT_FILENO, "\x1b[H", 3);
}
void die(const char *s) { //lo usamos para tirar excepciones
    cleanScreen();
    perror(s);
    exit(1);
}
void disableRawMode() {
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
void enableRawMode() {
    if(tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios; //estructura utilizada en Unix y sistemas Linux para especificar las opciones del terminal
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);  //ECHO:  each key you type to be printed to the terminal, so you can see what you’re typing, lo apagamos
    raw.c_cc[VMIN] = 0; //minimum number of bytes of input needed before read returns
    raw.c_cc[VTIME] = 1; // maximum amount of time to wait before read() returns.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); //se utiliza para aplicar los cambios que hicimos en la estructura raw al terminal correspondiente
}
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
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
    }
    return c;
}
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) 
        return -1;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}
struct abuf {
    char *b;
    int len;
};
#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}
void abFree(struct abuf *ab) {
    free(ab->b);
}

void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenRows; y++) {
        if (y == E.screenRows / 3) {
            char welcome[80];
            int welcomelen = snprintf(welcome, sizeof(welcome),
                "Welcome. Kilo editor -- version: %s", KILO_VERSION);
            if (welcomelen > E.screenCols) welcomelen = E.screenCols;
            int padding = (E.screenCols - welcomelen) / 2;
            if (padding) {
                abAppend(ab, "~", 1);
                padding--;
            }
            while (padding--) abAppend(ab, " ", 1);
            abAppend(ab, welcome, welcomelen);
        } else {
            abAppend(ab, "~", 1);
        }
        abAppend(ab, "\x1b[K", 3);
        if (y < E.screenRows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6); //hide cursor
    abAppend(&ab, "\x1b[H", 3); //move cursor at 0,0

    editorDrawRows(&ab);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);
    write(STDOUT_FILENO, ab.b, ab.len);
    
    abFree(&ab);
}
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
            E.cx--;
            }
            break;
        case ARROW_RIGHT:
            if (E.cx != E.screenCols - 1) {
            E.cx++;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) {
            E.cy--;
            }
            break;
        case ARROW_DOWN:
            if (E.cy != E.screenRows - 1) {
            E.cy++;
            }
            break;
    }
}
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            cleanScreen();
            exit(0);
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
            {
                int times = E.screenRows;
                while (times--)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            E.cx = E.screenCols - 1;
            break;
  }

}
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    if (getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
}
int main() {
    enableRawMode();
    initEditor();
    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

    return 0;
}
