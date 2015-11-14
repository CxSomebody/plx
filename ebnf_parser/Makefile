CXXFLAGS += -std=c++14 -Wall

ebnf: algorithm.o ebnf.o gen.o lexer.o parse.o
	c++ -o $@ $^

clean:
	rm -f ebnf *.o lexer.c

.PHONY: clean
