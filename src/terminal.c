#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "definitions.h"

/*** terminal ***/
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);  // Erase screen
  write(STDOUT_FILENO, "\x1b[H", 3);   // Put cursor at default position of 1,1

  perror(s);
  exit(1);
}

void cleanExit() {
  write(STDOUT_FILENO, "\x1b[2J", 4);  // Erase screen
  write(STDOUT_FILENO, "\x1b[H", 3);   // Put cursor at default position of 1,1
  exit(0);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) ==
      -1)  // Discard unhandled input and reset terminal settings to the
           // original
    die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
  atexit(disableRawMode);  // When program exits, raw mode gets disabled
  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP |
                   IXON);  // Disable input flags. BRKINT: Exit program, ICRNL:
                           // New Line, INPCK: Parity checking, ISTRIP: Strip
                           // 8th bit, IXON: Resume transmission
  raw.c_oflag &=
      ~(OPOST);          // Disable output flag. OPOST: Post processing output
  raw.c_cflag |= (CS8);  // Set control flag CS8: Use 8 bits per byte
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN |
                   ISIG);  // Disable local flags. ECHO: echo, ICANON: Turn off
                           // canonical mode (read input byte-by-byte), IEXTEN:
                           // Fix Ctrl-O, ISIG: Disable Ctrl-C and Ctrl-Z
  raw.c_cc[VMIN] = 0;  // Minimum bytes of input needed before read() can return
  raw.c_cc[VTIME] = 1;  // Maximum time to wait before read() returns

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
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
            case '1':
              return HOME_KEY;
            case '3':
              return DEL_KEY;
            case '4':
              return END_KEY;
            case '5':
              return PAGE_UP;
            case '6':
              return PAGE_DOWN;
            case '7':
              return HOME_KEY;
            case '8':
              return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A':
            return ARROW_UP;
          case 'B':
            return ARROW_DOWN;
          case 'C':
            return ARROW_RIGHT;
          case 'D':
            return ARROW_LEFT;
          case 'H':
            return HOME_KEY;
          case 'F':
            return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
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
