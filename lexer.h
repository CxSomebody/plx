extern char *tokstart;
extern int toklen;
extern int sym;
void lexer_open(const char *path);
void lexer_close(void);
void getsym(void);
