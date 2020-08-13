#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "definitions.h"
#include "find.h"
#include "highlight.h"
#include "history.h"
#include "output.h"
#include "rowOperations.h"
#include "terminal.h"

/*** editor operations ***/
void editorInsertChar(int c) {
  if (E.cy == E.numrows) {
    editorInsertRow(E.numrows, "", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx - COL_OFFSET, c);
  E.cx++;
}

void editorInsertNewline() {
  if (E.cx == COL_OFFSET) {
    editorInsertRow(E.cy, "", 0);
  } else {
    erow *row = &E.row[E.cy];
    editorInsertRow(E.cy + 1, &row->chars[E.cx - COL_OFFSET],
                    row->size - E.cx + COL_OFFSET);
    row = &E.row[E.cy];
    row->size = E.cx - COL_OFFSET;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
  }
  E.cy++;
  E.cx = COL_OFFSET;
}

void editorDelChar() {
  if (E.cy == E.numrows) return;
  if (E.cx == COL_OFFSET && E.cy == 0) return;
  erow *row = &E.row[E.cy];
  if (E.cx > COL_OFFSET) {
    editorRowDelChar(row, E.cx - 1 - COL_OFFSET);
    E.cx--;
  } else {
    E.cx = E.row[E.cy - 1].size + COL_OFFSET;
    editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
    editorDelRow(E.cy);
    E.cy--;
  }
}

/*** file i/o ***/
char *editorRowsToString(int *buflen) {
  int totlen = 0;
  int j;
  for (j = 0; j < E.numrows; j++) totlen += E.row[j].size + 1;
  *buflen = totlen;
  char *buf = malloc(totlen);
  char *p = buf;
  for (j = 0; j < E.numrows; j++) {
    memcpy(p, E.row[j].chars, E.row[j].size);
    p += E.row[j].size;
    *p = '\n';
    p++;
  }
  return buf;
}

void editorOpen(char *filename) {
  free(E.filename);
  E.filename = strdup(filename);

  editorSelectSyntaxHighlight();

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");
  char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    editorInsertRow(E.numrows, line, linelen);
  }
  free(line);
  fclose(fp);
  E.dirty = 0;
}

void editorSave() {
  if (E.filename == NULL) {
    E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
    if (E.filename == NULL) {
      editorSetStatusMessage("Save aborted");
      return;
    }
    editorSelectSyntaxHighlight();
  }
  int len;
  char *buf = editorRowsToString(&len);
  int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
  if (fd != -1) {
    if (ftruncate(fd, len) != -1) {
      if (write(fd, buf, len) == len) {
        close(fd);
        free(buf);
        E.dirty = 0;
        editorSetStatusMessage("%d bytes written to disk", len);
        return;
      }
    }
    close(fd);
  }
  free(buf);
  editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** command mode ***/
void commandCallback(char *command, int key) {
  if (key == '\r') {
    if (strcmp(command, "wq") == 0 || strcmp(command, "x") == 0) {
      editorSave();
      cleanExit();
    } else if (strcmp(command, "w") == 0) {
      editorSave();
    } else if (strcmp(command, "q") == 0) {
      if (E.dirty) {
        editorSetStatusMessage(
            "Dirty file, try :q! if you want to discard changes.");
        return;
      }
      cleanExit();
    } else if (strcmp(command, "q!") == 0 || strcmp(command, "q1") == 0) {
      cleanExit();
    } else {
      editorSetStatusMessage("no match");
    }
  }
}

void editorCommandMode() {
  char *command = editorPrompt("Command: %s", commandCallback);
  if (command) {
    free(command);
  }
  E.mode = NORMAL;
}

/*** input ***/
char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
  size_t bufsize = 128;
  char *buf = malloc(bufsize);
  size_t buflen = 0;
  buf[0] = '\0';
  while (1) {
    editorSetStatusMessage(prompt, buf);
    editorRefreshScreen();
    int c = editorReadKey();
    if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
      if (buflen != 0) buf[--buflen] = '\0';
    } else if (c == '\x1b') {
      editorSetStatusMessage("");
      if (callback) callback(buf, c);
      free(buf);
      return NULL;
    } else if (c == '\r') {
      if (buflen != 0) {
        editorSetStatusMessage("");
        if (callback) callback(buf, c);
        return buf;
      }
    } else if (!iscntrl(c) && c < 128) {
      if (buflen == bufsize - 1) {
        bufsize *= 2;
        buf = realloc(buf, bufsize);
      }
      buf[buflen++] = c;
      buf[buflen] = '\0';
    }
    if (callback) callback(buf, c);
  }
}

void editorMoveCursor(int key) {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  switch (key) {
    case 'h':
    case ARROW_LEFT:
      if (E.cx > COL_OFFSET) {
        E.cx--;
      } else if (E.cy > 0) {
        E.cy--;
        E.cx = E.row[E.cy].size + COL_OFFSET;
      }
      break;
    case 'l':
    case ARROW_RIGHT:
      if (row && E.cx < row->size + COL_OFFSET) {
        E.cx++;
      } else if (row && E.cx == row->size + COL_OFFSET) {
        E.cy++;
        E.cx = COL_OFFSET;
      }
      break;
    case 'k':
    case ARROW_UP:
      if (E.cy != 0) {
        E.cy--;
      }
      break;
    case 'j':
    case ARROW_DOWN:
      if (E.cy < E.numrows) {
        E.cy++;
      }
      break;
  }
  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  int rowlen = row ? row->size : 0;
  if (E.cx > rowlen + COL_OFFSET) {
    E.cx = rowlen + COL_OFFSET;
  }
}

void editorMoveCursorWord() {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  if (row == NULL) return;
  int position = E.cx - COL_OFFSET;
  int quantifier = E.command_quantifier ? E.command_quantifier : 1;
  char c;
  while (quantifier > 0) {
    if (position >= row->size) {
      E.cy++;
      E.cx = COL_OFFSET;
      quantifier--;
      if (quantifier < 1) return;
      E.command_quantifier = quantifier;
      editorMoveCursorWord();
      return;
    };
    c = row->chars[position];
    if (c < 'A' || c > 'z') {
      quantifier--;
    }
    position++;
  }
  if (position > row->size) {
    position = row->size;
  }
  E.cx = position + COL_OFFSET;
  E.command_quantifier = 0;
}

void editorMoveCursorBack() {
  erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  if (row == NULL) return;
  int position = E.cx - COL_OFFSET;
  int quantifier = E.command_quantifier ? E.command_quantifier : 1;
  char c;
  while (quantifier > 0) {
    position--;
    if (position < 0 && E.cy > 0) {
      E.cy--;
      E.cx = COL_OFFSET + E.row[E.cy].size;
      quantifier--;
      if (quantifier < 1) return;
      E.command_quantifier = quantifier;
      editorMoveCursorBack();
      return;
    };
    c = row->chars[position - 1];
    if (c < 'A' || c > 'z') {
      quantifier--;
    }
  }
  if (position < 0) {
    position = 0;
  }
  E.cx = position + COL_OFFSET;
  E.command_quantifier = 0;
}

void editorProcessSecondKey(char prevChar) {
  E.prevCommand = prevChar;
  editorRefreshScreen();

  char c = editorReadKey();

  switch (c) {
    case '\x1b':
      E.command_quantifier = 0;
      editorSetStatusMessage("");
      E.prevCommand = ' ';
      break;
    case 'd':
      if (prevChar == 'd') {
        editorDelRow(E.cy);
      }
      E.prevCommand = ' ';
      break;
    case 'g':
      if (prevChar == 'g') {
        if (E.command_quantifier > 0) {
          E.cy = E.command_quantifier > E.numrows ? E.numrows
                                                  : E.command_quantifier;
          E.command_quantifier = 0;
        } else {
          E.cy = 0;
          E.cx = COL_OFFSET;
        }
      }
      E.prevCommand = ' ';
      break;

    default:
      editorSetStatusMessage("Unknown command...");
      break;
  }
}

void editorProcessKeypress() {
  int c = editorReadKey();

  if (E.mode == NORMAL) {
    switch (c) {
      case '\x1b':
        E.command_quantifier = 0;
        editorSetStatusMessage("");
        break;
      case ':':
        E.mode = COMMAND;
        editorCommandMode();
        break;
      case 'i':
        E.mode = INSERT;
        editorSetStatusMessage("Insert mode");
        break;
      case 'j':
      case 'k':
      case 'h':
      case 'l':
        if (E.command_quantifier == 0) E.command_quantifier++;
        while (E.command_quantifier) {
          editorMoveCursor(c);
          E.command_quantifier--;
        }
        editorSetStatusMessage("");
        editorRefreshScreen();
        break;
      case BACKSPACE:
        editorMoveCursor(ARROW_LEFT);
        break;
      case 'w':
        editorMoveCursorWord();
        break;
      case 'b':
        editorMoveCursorBack();
        break;
      case '$':
        E.cx = E.row->size + COL_OFFSET;
        break;
      case 'o':
        E.cx = E.row[E.cy].size + COL_OFFSET;
        editorInsertNewline();
        E.mode = INSERT;
        break;
      case 'x':
        editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
      case 'a':
        editorMoveCursor(ARROW_RIGHT);
        E.mode = INSERT;
        break;
      case 'A':
        E.cx = E.row[E.cy].size + COL_OFFSET;
        E.mode = INSERT;
        break;
      case 'd':
        editorProcessSecondKey(c);
        break;
      case 'G':
        E.cy = E.numrows - 1;
        E.cx = COL_OFFSET;
        break;
      case 'g':
        editorProcessSecondKey(c);
        break;
      case '/':
        editorFind();
        break;
      case 'u':
        editorSetStatusMessage("Undo: %c", undo_history[E.undo_level.level].c);
        doUndo();
        break;
      case CTRL_KEY('r'):
        editorSetStatusMessage("Redo");
        doRedo();
        break;
      default:
        if (isdigit(c)) {
          if (E.command_quantifier > 0) {
            E.command_quantifier = E.command_quantifier * 10 + (c - '0');
          } else if (c == '0') {
            E.cx = COL_OFFSET;
            break;
          } else {
            E.command_quantifier = c - '0';
          }
          editorSetStatusMessage("Quanitfier: %d", E.command_quantifier);
        } else {
          editorSetStatusMessage("Unknown command: %c", c);
        }
        break;
    }
  } else if (E.mode == INSERT) {
    switch (c) {
      case '\x1b':
        E.mode = NORMAL;
        editorSetStatusMessage("Normal mode");
        break;
      case '\r':
        editorInsertNewline();
        break;

      case HOME_KEY:
        E.cx = COL_OFFSET;
        break;
      case END_KEY:
        if (E.cy < E.numrows) E.cx = E.row[E.cy].size + COL_OFFSET;
        break;

      case BACKSPACE:
      case CTRL_KEY('h'):
      case DEL_KEY:
        if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelChar();
        break;
      case PAGE_UP:
      case PAGE_DOWN: {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }

        int times = E.screenrows;
        while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      } break;
      case ARROW_UP:
      case ARROW_DOWN:
      case ARROW_LEFT:
      case ARROW_RIGHT:
        editorMoveCursor(c);
        break;
      case CTRL_KEY('l'):
      default:
        editorInsertChar(c);
        addUndo(c);
        break;
    }
  }
}
