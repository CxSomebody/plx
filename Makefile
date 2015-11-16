plx: plx.o lexer.o keywords.o parser.o
	c++ -o $@ $^

lexer_test: keywords.o lexer.o lexer_test.o
	cc -o $@ $^

keywords.c: keywords.gperf
	gperf -t -I -N gperf_keyword_sym $< > $@

clean:
	make -C ebnf_parser clean
	rm plx *.o keywords.{c,gperf{,.h}} tokens.h

parser.cpp: plx.ebnf
	ebnf_parser/ebnf -g $< > $@

tokens.h keywords.gperf keywords.gperf.h tokname.inc: tokens.in keywords.in
	./gen

lexer.o: lexer.c keywords.gperf.h tokname.inc
plx.o: plx.cpp lexer.h

.PHONY: clean
