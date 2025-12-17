# Makefile.am for the SDL portable network library
ACLOCAL_AMFLAGS = -I acinclude
AUTOMAKE_OPTIONS= foreign

lib_LTLIBRARIES = libSDL2_net.la

libSDL2_netincludedir = $(includedir)/SDL2
libSDL2_netinclude_HEADERS =		\
	SDL_net.h

libSDL2_net_la_SOURCES =		\
	SDLnet.c		\
	SDLnetTCP.c		\
	SDLnetUDP.c		\
	SDLnetselect.c		\
	SDLnetsys.h

EXTRA_DIST = \
	Android.mk		\
	CHANGES.txt 		\
	CMakeLists.txt		\
	LICENSE.txt 		\
	Makefile.os2		\
	README.txt 		\
	SDL2_net.spec		\
	SDL2_netConfig.cmake.in	\
	VisualC			\
	Xcode			\
	autogen.sh		\
	cmake			\
	mingw			\
	version.rc

libSDL2_net_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) $(LT_EXTRA)
libSDL2_net_la_LIBADD = @INETLIB@
if USE_VERSION_RC
libSDL2_net_la_DEPENDENCIES = version.o
endif

.rc.o:
	$(RC) $< $@

if ENABLE_EXAMPLES
EXAMPLES = showinterfaces chat chatd
noinst_PROGRAMS = $(EXAMPLES)
endif

showinterfaces_SOURCES = showinterfaces.c
showinterfaces_LDADD = libSDL2_net.la
chat_SOURCES = chat.c chat.h
chat_LDADD = libSDL2_net.la -lSDL2_test
chatd_SOURCES = chatd.c
chatd_LDADD = libSDL2_net.la

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: distcheck

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL2_net.pc

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(libdir)/cmake/SDL2_net
	$(INSTALL) -m 644 sdl2_net-config.cmake $(DESTDIR)$(libdir)/cmake/SDL2_net
	$(INSTALL) -m 644 sdl2_net-config-version.cmake $(DESTDIR)$(libdir)/cmake/SDL2_net

uninstall-hook:
	rm $(DESTDIR)$(libdir)/cmake/SDL2_net/sdl2_net-config.cmake
	rm $(DESTDIR)$(libdir)/cmake/SDL2_net/sdl2_net-config-version.cmake
	rm -r $(DESTDIR)$(libdir)/cmake/SDL2_net
