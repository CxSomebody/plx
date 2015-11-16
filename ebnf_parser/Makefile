CXXFLAGS += -std=c++14 -Wall

ebnf: algorithm.o ebnf.o gen.o lexer.o parse.o
	c++ -o $@ $^

preamble.inc: preamble.in
	./gen

algorithm.o: algorithm.cpp ebnf.h
ebnf.o: ebnf.cpp ebnf.h preamble.inc
gen.o: gen.o ebnf.h
parse.o: parse.cpp ebnf.h tokens.h

clean:
	rm -f ebnf *.o lexer.c

.PHONY: clean
