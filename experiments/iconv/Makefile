all: ic1

ic1: ic1.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

iconvpp.o: iconvpp.cpp
	g++  -Wall -Wextra -pedantic -std=c++11 -c -o $@ $<

clean:
	rm -f ic1 *.o

