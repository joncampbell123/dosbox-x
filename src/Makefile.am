AM_CPPFLAGS = -I$(top_srcdir)/include "-DRESDIR=\"$(resdir)\""

if EMSCRIPTEN
# fix https://github.com/emscripten-core/emscripten/issues/10061
dosbox_x_LDFLAGS = -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE=['$$ERRNO_CODES']
endif

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell builtin platform aviwriter output

bin_PROGRAMS = dosbox-x

resdir = $(datarootdir)/dosbox-x

res_DATA = \
	../dosbox-x.reference.conf \
	../dosbox-x.reference.full.conf \
	../contrib/fonts/FREECG98.BMP \
	../contrib/fonts/wqy_1?pt.bdf \
	../contrib/fonts/Nouveau_IBM.ttf \
	../contrib/fonts/SarasaGothicFixed.ttf \
	../CHANGELOG

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_x_SOURCES = dosbox.cpp $(ico_stuff)
dosbox_x_LDADD = debug/libdebug.a dos/libdos.a shell/libshell.a builtin/libbuiltin.a \
               ints/libints.a hardware/serialport/libserial.a hardware/parport/libparallel.a \
               libs/passthroughio/libpassthroughio.a gui/libgui.a libs/gui_tk/libgui_tk.a hardware/libhardware.a \
	       cpu/libcpu.a hardware/reSID/libresid.a fpu/libfpu.a gui/libgui.a \
		   misc/libmisc.a output/liboutput.a hardware/mame/libmame.a libs/zmbv/libzmbv.a libs/decoders/internal/libopusint.a

if !EMSCRIPTEN
dosbox_x_LDADD += aviwriter/libaviwriter.a
endif

if C_GAMELINK
if !EMSCRIPTEN
SUBDIRS += gamelink
dosbox_x_LDADD +=  gamelink/libgamelink.a
endif
endif

if C_DIRECT3D
dosbox_x_LDADD += output/direct3d/liboutputdirect3d.a
endif

if C_XBRZ
if !EMSCRIPTEN
dosbox_x_LDADD += libs/xBRZ/libxbrz.a
endif
endif

if C_MT32
if !EMSCRIPTEN
dosbox_x_LDADD += libs/mt32/libmt32.a
endif
endif

if !EMSCRIPTEN
dosbox_x_LDADD += libs/fluidsynth/libfluidsynth.a
endif

if EMSCRIPTEN
dosbox_x_js:
	cp dosbox-x dosbox-x.js

dosbox_x_js_install:
	sudo cp -v dosbox-x.js dosbox-x.wasm /var/www/html/em/
endif

EXTRA_DIST = winres.rc ../contrib/icons/dosbox-x.ico

# Ensure the macOS universal executables are correctly removed when cleaning
distclean-local:
	rm -f dosbox-x-arm64 dosbox-x-x86_64
