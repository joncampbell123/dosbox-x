AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/aviwriter -I$(top_srcdir)/src

SUBDIRS = 

EXTRA_DIST = opl.cpp opl.h adlib.h dbopl.h

noinst_LIBRARIES = libhardware.a

libhardware_a_SOURCES = adlib.cpp dma.cpp gameblaster.cpp hardware.cpp iohandler.cpp keyboard.cpp \
                        memory.cpp mixer.cpp pic.cpp timer.cpp \
			vga.cpp vga_attr.cpp vga_crtc.cpp vga_dac.cpp vga_draw.cpp vga_gfx.cpp vga_other.cpp \
			vga_memory.cpp vga_misc.cpp vga_seq.cpp vga_xga.cpp vga_s3.cpp vga_tseng.cpp vga_paradise.cpp \
			cmos.cpp dbopl.cpp \
			sn76496.h nukedopl.cpp pc98.cpp vga_pc98_gdc.cpp vga_pc98_dac.cpp vga_pc98_crtc.cpp vga_pc98_cg.cpp vga_pc98_egc.cpp 8255.cpp

