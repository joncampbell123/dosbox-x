AM_CPPFLAGS = -I$(top_srcdir)/include "-DRESDIR=\"$(resdir)\""

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell builtin platform output

bin_PROGRAMS = dosbox-x

resdir = $(datarootdir)/dosbox-x

res_DATA = \
	../dosbox.reference.conf \
	../font/FREECG98.BMP \
	../CHANGELOG

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_x_SOURCES = dosbox.cpp $(ico_stuff)
dosbox_x_LDADD = debug/libdebug.a dos/libdos.a shell/libshell.a builtin/libbuiltin.a \
               ints/libints.a misc/libmisc.a \
               gui/libgui.a libs/gui_tk/libgui_tk.a hardware/libhardware.a \
	       cpu/libcpu.a fpu/libfpu.a gui/libgui.a \
		   output/liboutput.a

EXTRA_DIST = winres.rc dosbox.ico


