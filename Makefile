CXXFLAGS += -std=c++14 -g -Wall

plx: codegen.o dataflow.o expr.o keywords.o lexer.o optimize.o parser.o plx.o regalloc.o symtab.o translate.o type.o
	c++ -o $@ $^

lexer_test: keywords.o lexer.o lexer_test.o
	cc -o $@ $^

keywords.c: keywords.gperf
	gperf -t -I -N gperf_keyword_sym $< > $@

tokens.h keywords.gperf keywords.gperf.h tokname.inc: tokens.in keywords.in
	./gen

codegen.o: codegen.cpp dynbitset.h dataflow.h translate.h
dataflow.o: dataflow.cpp dynbitset.h dataflow.h translate.h
expr.o: expr.cpp semant.h
lexer.o: lexer.c lexer.h tokens.h keywords.gperf.h tokname.inc
optimize.o: optimize.cpp translate.h dynbitset.h
symtab.o: symtab.cpp semant.h
parser.o: parser.cpp semant.h lexer.h tokens.h
plx.o: plx.cpp semant.h lexer.h tokens.h translate.h
regalloc.o: regalloc.cpp dynbitset.h dataflow.h translate.h
translate.o: translate.cpp translate.h semant.h dynbitset.h
type.o: type.cpp semant.h

clean:
	make -C ebnf_parser clean
	rm plx *.o keywords.{c,gperf{,.h}} tokens.h tokname.inc

.PHONY: clean
