all: chk

CFLAGS=-g -O2 -std=gnu++11
CXXFLAGS=-g -O2 -std=gnu++11

chk.o: chk.cpp
	g++ $(CXXFLAGS) -c -o $@ $<

chk: chk.o
	g++ -o $@ $<

clean:
	rm -fv chk chk.o

