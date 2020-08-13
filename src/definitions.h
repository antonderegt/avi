#ifndef DEFINITIONS_HEADER
#define DEFINITIONS_HEADER

#include <termios.h>
#include <time.h>

#include "history.h"

/*** defines ***/
#define AVI_VERSION "0.0.1"
#define AVI_TAB_STOP 8
#define COL_OFFSET 3

#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey {
  BACKSPACE = 127,
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

enum editorMode { NORMAL = 0, INSERT, VISUAL, COMMAND };

/*** data ***/
struct editorSyntax {
  char *filetype;
  char **filematch;
  char **keywords;
  char *singleline_comment_start;
  char *multiline_comment_start;
  char *multiline_comment_end;
  int flags;
};

typedef struct erow {
  int idx;
  int size;
  int rsize;
  char *chars;
  char *render;
  unsigned char *hl;
  int hl_open_comment;
} erow;

struct editorConfig {
  int cx, cy;  // Cursor position
  int rx;
  int rowoff;      // Row offset
  int coloff;      // Column offset
  int screenrows;  // Height of screen
  int screencols;  // Width of screen
  int numrows;     // Number of rows in file
  erow *row;
  int dirty;  // Indicates if file has been modified
  int mode;
  int command_quantifier;
  char prevCommand;
  history_level undo_level;
  history_level redo_level;
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct editorSyntax *syntax;
  struct termios orig_termios;
};
struct editorConfig E;

#endif
