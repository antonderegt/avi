/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** project includes ***/
#include "definitions.h"
#include "highlight.h"
#include "terminal.h"

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx) {
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    if (row->chars[j] == '\t') {
      rx += (AVI_TAB_STOP - 1) - (rx % AVI_TAB_STOP);
    }
    rx++;
  }
  return rx;
}

int editorRowRxToCx(erow *row, int rx) {
  int cur_rx = 0;
  int cx;
  for (cx = 0; cx < row->size; cx++) {
    if (row->chars[cx] == '\t')
      cur_rx += (AVI_TAB_STOP - 1) - (cur_rx % AVI_TAB_STOP);
    cur_rx++;
    if (cur_rx > rx) return cx;
  }
  return cx;
}

void editorUpdateRow(erow *row) {
  int tabs = 0;
  int j;
  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t') tabs++;
  free(row->render);
  row->render = malloc(row->size + tabs * (AVI_TAB_STOP - 1) + 1);
  int idx = 0;
  for (j = 0; j < row->size; j++) {
    if (row->chars[j] == '\t') {
      row->render[idx++] = ' ';
      while (idx % AVI_TAB_STOP != 0) row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  row->rsize = idx;
  editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
  if (at < 0 || at > E.numrows) return;
  E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
  memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
  for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;

  E.row[at].idx = at;

  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  memcpy(E.row[at].chars, s, len);
  E.row[at].chars[len] = '\0';

  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  E.row[at].hl = NULL;
  E.row[at].hl_open_comment = 0;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  E.dirty++;
}

void editorFreeRow(erow *row) {
  free(row->render);
  free(row->chars);
  free(row->hl);
}
void editorDelRow(int at) {
  if (at < 0 || at >= E.numrows) return;
  editorFreeRow(&E.row[at]);
  memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
  E.numrows--;
  E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
  if (at < 0 || at > row->size) at = row->size;
  row->chars = realloc(row->chars, row->size + 2);
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
  row->chars = realloc(row->chars, row->size + len + 1);
  memcpy(&row->chars[row->size], s, len);
  row->size += len;
  row->chars[row->size] = '\0';
  editorUpdateRow(row);
  E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
  if (at < 0 || at >= row->size) return;
  memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
  row->size--;
  editorUpdateRow(row);
  E.dirty++;
}

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

/*** find ***/
void editorFindCallback(char *query, int key) {
  static int last_match = -1;
  static int direction = 1;

  static int saved_hl_line;
  static char *saved_hl = NULL;
  if (saved_hl) {
    memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
    free(saved_hl);
    saved_hl = NULL;
  }

  if (key == '\r' || key == '\x1b') {
    last_match = -1;
    direction = 1;
    return;
  } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
    direction = 1;
  } else if (key == ARROW_LEFT || key == ARROW_UP) {
    direction = -1;
  } else {
    last_match = -1;
    direction = 1;
  }
  if (last_match == -1) direction = 1;
  int current = last_match;
  int i;
  for (i = 0; i < E.numrows; i++) {
    current += direction;
    if (current == -1)
      current = E.numrows - 1;
    else if (current == E.numrows)
      current = 0;
    erow *row = &E.row[current];
    char *match = strstr(row->render, query);
    if (match) {
      last_match = current;
      E.cy = current;
      E.cx = editorRowRxToCx(row, match - row->render) + COL_OFFSET;
      E.rowoff = E.numrows;

      saved_hl_line = current;
      saved_hl = malloc(row->rsize);
      memcpy(saved_hl, row->hl, row->rsize);
      memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
      break;
    }
  }
}

void editorFind() {
  int saved_cx = E.cx;
  int saved_cy = E.cy;
  int saved_coloff = E.coloff;
  int saved_rowoff = E.rowoff;

  char *query =
      editorPrompt("Search: %s (Use ESC/Arrows/Enter)", editorFindCallback);

  if (query) {
    free(query);
  } else {
    E.cx = saved_cx;
    E.cy = saved_cy;
    E.coloff = saved_coloff;
    E.rowoff = saved_rowoff;
  }
}

/*** append buffer ***/
struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL) return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/*** output ***/
void editorScroll() {
  E.rx = 0;
  if (E.cy < E.numrows) {
    E.rx = editorRowCxToRx(&E.row[E.cy], E.cx - COL_OFFSET) + COL_OFFSET;
  }
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.coloff) {
    E.coloff = E.rx;
  }
  if (E.rx >= E.coloff + E.screencols) {
    E.coloff = E.rx - E.screencols + 1;
  }
}

void editorDrawLineNumbers(char *row, int filerow) {
  int rowNumber;
  if (filerow < E.cy) {
    rowNumber = E.cy - filerow;
  } else if (filerow > E.cy) {
    rowNumber = filerow - E.cy;
  } else {
    rowNumber = 0;
  }
  row[0] = '0' + rowNumber / 10;
  if (row[0] == '0') row[0] = ' ';
  row[1] = '0' + rowNumber % 10;
  row[2] = ' ';
}

void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;
    if (filerow >= E.numrows) {
      if (E.numrows == 0 && y == E.screenrows / 3) {
        char welcome[80];
        int welcomelen = snprintf(welcome, sizeof(welcome),
                                  "Avi editor -- version %s", AVI_VERSION);
        if (welcomelen > E.screencols) welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--) abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      char *c = &E.row[filerow].render[E.coloff];
      unsigned char *hl = &E.row[filerow].hl[E.coloff];
      int current_color = -1;
      int j;
      char row[2];
      editorDrawLineNumbers(row, filerow);
      abAppend(ab, row, 3);
      for (j = 0; j < len; j++) {
        if (iscntrl(c[j])) {
          char sym = (c[j] <= 26) ? '@' + c[j] : '?';
          abAppend(ab, "\x1b[7m", 4);
          abAppend(ab, &sym, 1);
          abAppend(ab, "\x1b[m", 3);
          if (current_color != -1) {
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
            abAppend(ab, buf, clen);
          }
        } else if (hl[j] == HL_NORMAL) {
          if (current_color != -1) {
            abAppend(ab, "\x1b[39m", 5);
            current_color = -1;
          }
          abAppend(ab, &c[j], 1);
        } else {
          int color = editorSyntaxToColor(hl[j]);
          if (color != current_color) {
            current_color = color;
            char buf[16];
            int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
            abAppend(ab, buf, clen);
          }
          abAppend(ab, &c[j], 1);
        }
      }
      abAppend(ab, "\x1b[39m", 5);
    }
    abAppend(ab, "\x1b[K", 3);
    abAppend(ab, "\r\n", 2);
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  abAppend(ab, "\x1b[7m", 4);
  char status[80], rstatus[80];
  int len = snprintf(status, sizeof(status), "%.20s - %d lines %s %s",
                     E.filename ? E.filename : "[No Name]", E.numrows,
                     E.dirty ? "(modified)" : "",
                     E.mode == INSERT
                         ? "--INSERT--"
                         : E.mode == NORMAL ? "--NORMAL--" : "--COMMAND--");
  int rlen =
      E.command_quantifier == 0
          ? snprintf(rstatus, sizeof(rstatus), "%c | %s | %d/%d", E.prevCommand,
                     E.syntax ? E.syntax->filetype : "no ft", E.cy + 1,
                     E.numrows)
          : snprintf(rstatus, sizeof(rstatus), "%d%c | %s | %d/%d",
                     E.command_quantifier, E.prevCommand,
                     E.syntax ? E.syntax->filetype : "no ft", E.cy + 1,
                     E.numrows);
  if (len > E.screencols) len = E.screencols;
  abAppend(ab, status, len);
  while (len < E.screencols + COL_OFFSET) {
    if (E.screencols + COL_OFFSET - len == rlen) {
      abAppend(ab, rstatus, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m", 3);
  abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols) msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  editorScroll();
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6);
  abAppend(&ab, "\x1b[H", 3);
  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
           (E.rx - E.coloff) + 1);
  abAppend(&ab, buf, strlen(buf));
  abAppend(&ab, "\x1b[?25h", 6);
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  va_end(ap);
  E.statusmsg_time = time(NULL);
}

/*** history ***/
struct history_action {
  int uy;
  int ux;
  int c;
  int end;
};

struct history_action undo_history[MAX_HISTORY];
struct history_action redo_history[MAX_HISTORY];

void addUndo(char c) {
  if (E.undo_level.level >= MAX_HISTORY) {
    E.undo_level.level = 0;
    E.undo_level.wraps++;
  };

  undo_history[E.undo_level.level].c = c;
  undo_history[E.undo_level.level].uy = E.cy;
  undo_history[E.undo_level.level].ux = E.cx;
  undo_history[E.undo_level.level].end = 0;
  E.undo_level.level++;
}

void addRedo(char c) {
  if (E.redo_level.level >= MAX_HISTORY) {
    E.redo_level.level = 0;
    E.redo_level.wraps++;
  }

  redo_history[E.redo_level.level].c = c;
  redo_history[E.redo_level.level].uy = E.cy;
  redo_history[E.redo_level.level].ux = E.cx;
  redo_history[E.redo_level.level].end = 0;
  E.redo_level.level++;
}

void doUndo() {
  if (E.undo_level.level <= 0 && E.undo_level.wraps <= 0) return;
  if (E.undo_level.level <= 0 && E.undo_level.wraps > 0) {
    E.undo_level.level = MAX_HISTORY;
    E.undo_level.wraps = 0;
  }
  E.undo_level.level--;
  if (undo_history[E.undo_level.level].end == 1) return;
  undo_history[E.undo_level.level].end = 1;
  editorSetStatusMessage("level %d, wraps: %d", E.undo_level.level,
                         E.undo_level.wraps);
  E.cy = undo_history[E.undo_level.level].uy;
  E.cx = undo_history[E.undo_level.level].ux;
  char toRemove = E.row[E.cy].chars[E.cx - COL_OFFSET - 1];
  editorDelChar();
  addRedo(toRemove);
}

void doRedo() {
  if (E.redo_level.level <= 0 && E.redo_level.wraps <= 0) return;
  if (E.redo_level.level <= 0 && E.redo_level.wraps > 0) {
    E.redo_level.level = MAX_HISTORY;
    E.redo_level.wraps = 0;
  }
  E.redo_level.level--;
  if (redo_history[E.redo_level.level].end == 1) return;
  redo_history[E.redo_level.level].end = 1;
  E.cy = redo_history[E.redo_level.level].uy;
  E.cx = redo_history[E.redo_level.level].ux;
  char toInsert = redo_history[E.redo_level.level].c;
  editorInsertChar(toInsert);
  addUndo(toInsert);
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

/*** init ***/
void initEditor() {
  E.cx = COL_OFFSET;
  E.cy = 0;
  E.rx = COL_OFFSET;
  E.rowoff = 0;
  E.coloff = 0;
  E.numrows = 0;
  E.row = NULL;
  E.dirty = 0;
  E.mode = NORMAL;
  E.command_quantifier = 0;
  E.prevCommand = ' ';
  E.undo_level.level = 0;
  E.undo_level.wraps = 0;
  E.redo_level.level = 0;
  E.redo_level.wraps = 0;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;
  E.syntax = NULL;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
  E.screenrows -= 2;
  E.screencols -= COL_OFFSET;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage(
      "HELP: i = INSERT MODE | ESC = NORMAL MODE | :q = QUIT | / = find");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
