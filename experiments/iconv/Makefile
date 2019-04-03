all: ic1 ic2 ic3 ic4 ic5 icw1

ic1: ic1.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

ic2: ic2.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

ic3: ic3.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

ic4: ic4.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

ic5: ic5.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

icw1: icw1.cpp iconvpp.o
	g++ -Wall -Wextra -pedantic -std=c++11 -o $@ $< iconvpp.o -liconv

iconvpp.o: iconvpp.cpp
	g++  -Wall -Wextra -pedantic -std=c++11 -c -o $@ $<

clean:
	rm -f ic1 ic2 ic3 ic4 ic5 icw1 *.o

