/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include "definitions.h"
#include "find.h"
#include "highlight.h"
#include "history.h"
#include "input.h"
#include "output.h"
#include "rowOperations.h"
#include "terminal.h"

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
