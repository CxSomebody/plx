plx: plx.o lexer.o keywords.o symtab.o parser.o
	c++ -o $@ $^

lexer_test: keywords.o lexer.o lexer_test.o
	cc -o $@ $^

keywords.c: keywords.gperf
	gperf -t -I -N gperf_keyword_sym $< > $@

parser.cpp: plx.ebnf
	ebnf_parser/ebnf -g $< > $@

tokens.h keywords.gperf keywords.gperf.h tokname.inc: tokens.in keywords.in
	./gen

lexer.o: lexer.c lexer.h tokens.h keywords.gperf.h tokname.inc
symtab.o: symtab.cpp symtab.h
plx.o: plx.cpp lexer.h tokens.h

clean:
	make -C ebnf_parser clean
	rm plx *.o keywords.{c,gperf{,.h}} tokens.h tokname.inc parser.cpp

.PHONY: clean
