CXXFLAGS += -std=c++14

ebnf: lexer.o ebnf.o
	c++ -o $@ $^

clean:
	rm -f ebnf *.o lexer.c

.PHONY: clean
