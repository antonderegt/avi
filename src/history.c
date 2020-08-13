#include "history.h"

#include "definitions.h"
#include "input.h"
#include "output.h"

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
