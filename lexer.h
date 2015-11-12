extern char *token_start;
extern int token_len;
extern int sym;
#include "tokens.h"
void lexer_open(char *path);
void lexer_close(void);
void lex(void);
