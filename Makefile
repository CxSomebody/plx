plx: plx.o lexer.o keywords.o
	cc -o $@ $^

keywords.c: keywords.gperf
	gperf -t -I -N gperf_keyword_sym $< > $@

clean:
	make -C ebnf_parser clean
	rm plx *.o keywords.{c,gperf{,.h}} tokens.h
