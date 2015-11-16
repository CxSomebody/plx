extern char *tokstart;
extern int toklen;
extern int sym;
#include "tokens.h"
void lexer_open(const char *path);
void lexer_close(void);
void getsym(void);
