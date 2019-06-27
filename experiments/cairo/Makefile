all: cairo1 cairo2 cairo3 cairo4

cairo_cflags=$(shell pkg-config cairo --cflags)
cairo_libs=$(shell pkg-config cairo --libs)

sdl2_cflags=$(shell pkg-config sdl2 --cflags)
sdl2_libs=$(shell pkg-config sdl2 --libs)

cairo1: cairo1.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 $(cairo_cflags) $(sdl2_cflags) -o $@ $< -liconv $(cairo_libs) $(sdl2_libs)

cairo2: cairo2.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 $(cairo_cflags) $(sdl2_cflags) -o $@ $< -liconv $(cairo_libs) $(sdl2_libs)

cairo3: cairo3.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 $(cairo_cflags) $(sdl2_cflags) -o $@ $< -liconv $(cairo_libs) $(sdl2_libs)

cairo4: cairo4.cpp
	g++ -Wall -Wextra -pedantic -std=c++11 $(cairo_cflags) $(sdl2_cflags) -o $@ $< -liconv $(cairo_libs) $(sdl2_libs)

clean:
	rm -f cairo1 cairo2 cairo3 cairo4 *.o

