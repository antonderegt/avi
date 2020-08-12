#ifndef HIGHLIGHT_HEADER
#define HIGHLIGHT_HEADER

#include "definitions.h"

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

void editorUpdateSyntax(erow *);
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int);

#endif
