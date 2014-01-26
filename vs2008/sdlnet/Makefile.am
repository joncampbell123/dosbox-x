# Makefile.am for the SDL sample image loading library and viewer

lib_LTLIBRARIES = libSDL_net.la

libSDL_netincludedir = $(includedir)/SDL
libSDL_netinclude_HEADERS =		\
	SDL_net.h

libSDL_net_la_SOURCES =		\
	SDLnet.c		\
	SDLnetTCP.c		\
	SDLnetUDP.c		\
	SDLnetselect.c		\
	SDLnetsys.h

EXTRA_DIST =			\
	CHANGES			\
	version.rc		\
	VisualC			\
	Xcode			\
	Xcode-iOS		\
	MPWmake.sea.bin		\
	Watcom-OS2.zip		\
	quit.bmp		\
	scroll_dn.bmp		\
	scroll_up.bmp		\
	SDL_net.spec		\
	SDL_net.spec.in		\
	gcc-fat.sh		\
	autogen.sh

if USE_VERSION_RC
libSDL_net_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -Wl,version.o
libSDL_net_la_LIBADD = @INETLIB@
libSDL_net_la_DEPENDENCIES = version.o
else
libSDL_net_la_LDFLAGS = 	\
	-no-undefined		\
	-release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
libSDL_net_la_LIBADD = @INETLIB@
endif

%.o : %.rc
	$(WINDRES) $< $@

if HAVE_GUI_LIB
EXAMPLES = showinterfaces chat chatd
else
EXAMPLES = showinterfaces
endif
noinst_PROGRAMS = $(EXAMPLES)

showinterfaces_SOURCES = showinterfaces.c
showinterfaces_LDADD = libSDL_net.la
chat_SOURCES = chat.cpp chat.h
chat_LDADD = libSDL_net.la -lGUI
chatd_SOURCES = chatd.c
chatd_LDADD = libSDL_net.la

# Rule to build tar-gzipped distribution package
$(PACKAGE)-$(VERSION).tar.gz: distcheck

# Rule to build RPM distribution package
rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL_net.pc
