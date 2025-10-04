all: lua1

lua1: lua1.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< -llua -ldl

clean:
	rm -f lua1

