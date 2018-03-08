AM_CPPFLAGS = -I$(top_srcdir)/include "-DRESDIR=\"$(resdir)\""

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell builtin platform mt32 aviwriter

bin_PROGRAMS = dosbox-x

resdir = $(datarootdir)/dosbox-x

res_DATA = \
	../font/FREECG98.BMP

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_x_SOURCES = dosbox.cpp $(ico_stuff)
dosbox_x_LDADD = debug/libdebug.a dos/libdos.a shell/libshell.a builtin/libbuiltin.a \
               ints/libints.a misc/libmisc.a hardware/serialport/libserial.a hardware/parport/libparallel.a \
               libs/porttalk/libporttalk.a gui/libgui.a libs/gui_tk/libgui_tk.a hardware/libhardware.a \
	       cpu/libcpu.a hardware/reSID/libresid.a fpu/libfpu.a gui/libgui.a aviwriter/libaviwriter.a

if C_MT32
dosbox_x_LDADD += mt32/libmt32.a
endif

EXTRA_DIST = winres.rc dosbox.ico


