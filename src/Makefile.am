AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = cpu debug dos fpu gui hardware libs ints misc shell platform 

bin_PROGRAMS = dosbox

if HAVE_WINDRES
ico_stuff = winres.rc
endif

.rc.o:
	$(WINDRES) -o $@ $<

dosbox_SOURCES = dosbox.cpp save_state.cpp save_state.h miniunz.c minizip.c unzip.c zip.c iowin32.c ioapi.c mztools.c $(ico_stuff)
dosbox_LDADD = cpu/libcpu.a debug/libdebug.a dos/libdos.a fpu/libfpu.a hardware/libhardware.a shell/libshell.a gui/libgui.a \
               ints/libints.a misc/libmisc.a hardware/serialport/libserial.a hardware/parport/libparallel.a \
               libs/gui_tk/libgui_tk.a libs/porttalk/libporttalk.a \
               hardware/reSID/libresid.a

EXTRA_DIST = winres.rc dosbox.ico


