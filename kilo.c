#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}
void enableRawMode() {

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disableRawMode);

    struct termios raw = orig_termios; //estructura utilizada en Unix y sistemas Linux para especificar las opciones del terminal
      raw.c_lflag &= ~(ECHO | ICANON);  //ECHO:  each key you type to be printed to the terminal, so you can see what you’re typing, lo apagamos
    
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw); //se utiliza para aplicar los cambios que hicimos en la estructura raw al terminal correspondiente
}

int main() {
    enableRawMode();
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
        if(iscntrl(c)) { //tests whether a character is a control character. Control characters are nonprintable characters
            printf("%d\n", c);
        } else {
            printf("%d ('%c')\n", c, c);
        }
    }

    return 0;
}