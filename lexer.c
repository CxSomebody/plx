#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "lexer.h"
#include "keywords.gperf.h"

#define BUFFER_SIZE 0x1000

#define MAX_TOKEN_LEN 0x100

FILE *fp;

char buf[BUFFER_SIZE];
int bufp = BUFFER_SIZE-1;

char *token_start;
int token_len;

int sym;

//int lineno;

static void fillbuf(void)
{
	// -1 for terminating NUL
	int leftover = (BUFFER_SIZE-1)-bufp;
	memmove(buf, buf+bufp, leftover);
	bufp = leftover;
	fread(buf, 1, (BUFFER_SIZE-1)-leftover, fp);
	// TODO check EOF
}

struct keyword_sym *gperf_keyword_sym();

static int keyword_sym(char *keyword, int len)
{
	struct keyword_sym *ks = gperf_keyword_sym(keyword, len);
	return ks ? ks->sym : IDENT;
}

void lex(void)
{
	char *p;
	for (;;) {
		p = buf+bufp;
		// skip whitespace... including comments
		for (;;) {
			while (isspace(*p))
				p++;
			if (p[0] != '/' || p[1] != '/')
				break;
			p = strchr(p+2, '\n');
			// TODO
			assert(p);
		}
		if ((p-buf) + MAX_TOKEN_LEN < BUFFER_SIZE)
			break;
		fillbuf();
	}
	token_start = p;
	if (isalpha(*p)) {
		char *q = p+1;
		while (isalnum(*q))
			q++;
		char saved = *q;
		*q = 0;
		sym = keyword_sym(p, q-p);
		*q = saved;
		p = q;
	} else if (isdigit(*p)) {
		char *q = p+1;
		while (isdigit(*q))
			q++;
		sym = INT;
		p = q;
	} else {
		sym = *(unsigned char *)p;
		if (sym) {
			switch (*p++) {
			case ':':
				if (*p == '=') {
					p++;
					sym = BECOMES;
				}
			}
		}
	}
	token_len = p-token_start;
	bufp = p-buf;
}

void lexer_open(char *path)
{
	fp = fopen(path, "r");
	assert(fp);
}

void lexer_close(void)
{
	fclose(fp);
}
