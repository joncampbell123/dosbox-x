all: opgen

opgt.o: opgt.c
	gcc -Wall -Wextra -pedantic -std=c11 -c -o $@ $<

opgen.o: opgen.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 -c -o $@ $<

opgen: opgen.o opgt.o
	g++ -o $@ opgen.o opgt.o

clean:
	rm -f opgen *.o

