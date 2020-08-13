#ifndef HIGHLIGHT_HEADER
#define HIGHLIGHT_HEADER

#include "definitions.h"

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))
#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

enum editorHighlight {
  HL_NORMAL = 0,
  HL_COMMENT,
  HL_MLCOMMENT,
  HL_KEYWORD1,
  HL_KEYWORD2,
  HL_STRING,
  HL_NUMBER,
  HL_MATCH
};

void editorUpdateSyntax(erow *);
void editorSelectSyntaxHighlight();
int editorSyntaxToColor(int);

#endif
