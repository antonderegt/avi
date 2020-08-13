#ifndef INPUT_HEADER
#define INPUT_HEADER

void editorProcessKeypress();
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorDelChar();
void editorInsertChar(int c);
void editorOpen(char *filename);

#endif
