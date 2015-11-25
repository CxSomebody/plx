extern char *tokstart;
extern int toklen;
extern union tokval_u {
	char *s;
	int i;
} tokval;
extern int lineno, colno;
void lexer_open(const char *path);
void lexer_close(void);
int lex(void);
