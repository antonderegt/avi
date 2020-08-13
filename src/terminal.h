#ifndef TERMINAL_HEADER
#define TERMINAL_HEADER

void die(const char *s);
void cleanExit();
void enableRawMode();
int editorReadKey();
int getWindowSize(int *rows, int *cols);

#endif
