ACLOCAL_AMFLAGS = -I acinclude
AUTOMAKE_OPTIONS= foreign

# Build static library only
lib_LIBRARIES = libSDL2_net.a

libSDL2_netincludedir = $(includedir)/SDL2
libSDL2_netinclude_HEADERS = SDL_net.h

libSDL2_net_a_SOURCES = \
	SDLnet.c \
	SDLnetTCP.c \
	SDLnetUDP.c \
	SDLnetselect.c \
	SDLnetsys.h

EXTRA_DIST = \
	Android.mk \
	CHANGES.txt \
	CMakeLists.txt \
	LICENSE.txt \
	Makefile.os2 \
	README.txt \
	SDL2_net.spec \
	SDL2_netConfig.cmake.in \
	VisualC \
	Xcode \
	autogen.sh \
	cmake \
	mingw \
	version.rc

.rc.o:
	$(RC) $< $@

# Distribution rules
$(PACKAGE)-$(VERSION).tar.gz: distcheck

rpm: $(PACKAGE)-$(VERSION).tar.gz
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL2_net.pc

install-data-local:
	$(MKDIR_P) $(DESTDIR)$(libdir)/cmake/SDL2_net
	$(INSTALL) -m 644 sdl2_net-config.cmake $(DESTDIR)$(libdir)/cmake/SDL2_net
	$(INSTALL) -m 644 sdl2_net-config-version.cmake $(DESTDIR)$(libdir)/cmake/SDL2_net

uninstall-hook:
	rm -f $(DESTDIR)$(libdir)/cmake/SDL2_net/sdl2_net-config.cmake
	rm -f $(DESTDIR)$(libdir)/cmake/SDL2_net/sdl2_net-config-version.cmake
	rmdir --ignore-fail-on-non-empty $(DESTDIR)$(libdir)/cmake/SDL2_net