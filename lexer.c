#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "keywords.gperf.h"

#define BUFFER_SIZE 0x1000

#define MAX_TOKEN_LEN 0x100

static FILE *fp;
static int eof;
char *fpath;

static char buf[BUFFER_SIZE]; // buf[BUFFER_SIZE-1] is always 0
static char *bufp = buf+(BUFFER_SIZE-1);

char *token_start;
int token_len;

int sym;

int lineno;
int colno;

static void fillbuf(void)
{
	assert(!eof);
	// -1 for terminating NUL
	int leftover = buf+(BUFFER_SIZE-1)-bufp;
	memmove(buf, bufp, leftover);
	bufp = buf;
	int ntoread = (BUFFER_SIZE-1)-leftover;
	int nread = fread(buf+leftover, 1, ntoread, fp);
	if (nread < ntoread) {
		eof = 1;
		buf[leftover+nread] = 0;
	}
}

struct keyword_sym *gperf_keyword_sym();

/* skip to '\n' */
static void skip_line(void)
{
	for (;;) {
		char *q = strchr(bufp, '\n');
		if (q) {
			bufp = q;
			break;
		}
		bufp = buf+(BUFFER_SIZE-1);
		if (!eof)
			fillbuf();
	}
}

static void error(char *msg)
{
	fprintf(stderr, "%s: %d:%d: %s\n", fpath, lineno, colno, msg);
}

void getsym(void)
{
	colno += token_len;
	char *p;
	char errflag = 0;
	do {
		if (errflag) {
			skip_line();
			errflag = 0;
		}
		for (;;) {
			p = bufp;
			// skip whitespace... including comments
			for (;;) {
				while (isspace(*p)) {
					if (*p == '\n') {
						lineno++;
						colno = 0;
					}
					p++;
					colno++;
				}
				if (p[0] != '/' || p[1] != '/')
					break;
				bufp = p;
				skip_line();
				p = bufp;
			}
			bufp = p;
			if ((p-buf) + MAX_TOKEN_LEN < BUFFER_SIZE || eof)
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
			struct keyword_sym *ks = gperf_keyword_sym(p, q-p);
			sym = ks ? ks->sym : IDENT;
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
					char *q;
				case ':':
					if (*p == '=') {
						p++;
						sym = BECOMES;
					}
					break;
				case '<':
					if (*p == '=') {
						p++;
						sym = LE;
					} else if (*p == '>') {
						p++;
						sym = NE;
					}
					break;
				case '>':
					if (*p == '=') {
						p++;
						sym = GE;
					}
					break;
				case '\'':
					q = strchr(p, '\'');
					if (q) {
						p = q+1;
						sym = CHAR;
					} else {
						errflag = 1;
						error("unmatched '");
					}
					break;
				case '"':
					q = strchr(p, '"');
					if (q) {
						p = q+1;
						sym = STRING;
					} else {
						errflag = 1;
						error("unmatched \"");
					}
				}
			}
		}
	} while (errflag);
	token_len = p-token_start;
	bufp = p;
}

void lexer_open(char *path)
{
	assert(!fp);
	fp = fopen(path, "r");
	assert(fp);
	lineno = 1;
	fpath = strdup(path);
}

void lexer_close(void)
{
	fclose(fp);
	free(fpath);
}
