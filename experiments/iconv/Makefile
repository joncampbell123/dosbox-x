all: iconvpp

iconvpp: iconvpp.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< -liconv

clean:
	rm -f iconvpp

