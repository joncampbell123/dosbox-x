AM_CPPFLAGS = -I$(top_srcdir)/include

SUBDIRS = serialport

EXTRA_DIST = opl.cpp opl.h adlib.h dbopl.h

noinst_LIBRARIES = libhardware.a

libhardware_a_SOURCES = adlib.cpp dma.cpp gameblaster.cpp hardware.cpp iohandler.cpp joystick.cpp keyboard.cpp \
                        memory.cpp mixer.cpp pcspeaker.cpp pic.cpp sblaster.cpp tandy_sound.cpp timer.cpp \
			vga.cpp vga_attr.cpp vga_crtc.cpp vga_dac.cpp vga_draw.cpp vga_gfx.cpp vga_other.cpp \
			vga_memory.cpp vga_misc.cpp vga_seq.cpp vga_xga.cpp vga_s3.cpp vga_tseng.cpp vga_paradise.cpp \
			cmos.cpp disney.cpp gus.cpp mpu401.cpp ipx.cpp ipxserver.cpp dbopl.cpp ide.cpp


