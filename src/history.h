#ifndef HISTORY_HEADER
#define HISTORY_HEADER

#define MAX_HISTORY 1000

typedef struct history_level {
  int level;
  int wraps;
} history_level;

struct history_action {
  int uy;
  int ux;
  int c;
  int end;
};

struct history_action undo_history[MAX_HISTORY];
struct history_action redo_history[MAX_HISTORY];

void addUndo(char c);
void doRedo();
void doUndo();

#endif
