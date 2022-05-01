AM_CPPFLAGS = -I$(top_srcdir)/include "-DRESDIR=\"$(resdir)\""

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell builtin platform aviwriter output

bin_PROGRAMS = dosbox-x

resdir = $(datarootdir)/dosbox-x

res_DATA = \
	../dosbox-x.reference.conf \
	../dosbox-x.reference.full.conf \
	../contrib/fonts/FREECG98.BMP \
	../CHANGELOG

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_x_SOURCES = dosbox.cpp $(ico_stuff)
dosbox_x_LDADD = debug/libdebug.a dos/libdos.a shell/libshell.a builtin/libbuiltin.a \
               ints/libints.a hardware/serialport/libserial.a hardware/parport/libparallel.a \
               libs/porttalk/libporttalk.a gui/libgui.a libs/gui_tk/libgui_tk.a hardware/libhardware.a \
	       cpu/libcpu.a hardware/reSID/libresid.a fpu/libfpu.a gui/libgui.a \
		   misc/libmisc.a output/liboutput.a hardware/mame/libmame.a libs/zmbv/libzmbv.a libs/decoders/internal/libopusint.a

if !EMSCRIPTEN
dosbox_x_LDADD += aviwriter/libaviwriter.a
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
	emcc -s TOTAL_MEMORY=134217728 -s DISABLE_EXCEPTION_CATCHING=0 -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1 -s EMTERPRETIFY_FILE=dosbox-x.data $(dosbox_x_OBJECTS) $(dosbox_x_LDADD) $(LIBS) $(dosbox_x_OBJECTS) $(dosbox_x_DEPENDENCIES) $(EXTRA_dosbox_x_DEPENDENCIES) -o dosbox-x.html

dosbox_x_js_install:
	sudo cp -v dosbox-x.data dosbox-x.html dosbox-x.js dosbox-x.wasm /var/www/html/em/
endif

EXTRA_DIST = winres.rc ../contrib/icons/dosbox-x.ico


