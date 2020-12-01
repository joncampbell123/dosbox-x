AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/aviwriter -I$(top_srcdir)/src/hardware/snd_pc98/sound -I$(top_srcdir)/src/hardware/snd_pc98/common -I$(top_srcdir)/src/hardware/snd_pc98/generic -I$(top_srcdir)/src/hardware/snd_pc98/sound/getsnd -I$(top_srcdir)/src/hardware/snd_pc98/x11 -I$(top_srcdir)/src/hardware/snd_pc98/cbus -I$(top_srcdir)/src

SUBDIRS = serialport parport reSID mame

EXTRA_DIST = opl.cpp opl.h adlib.h dbopl.h pci_devices.h voodoo_types.h voodoo_def.h voodoo_data.h \
             voodoo_interface.h voodoo_emu.h voodoo_vogl.h voodoo_opengl.h

noinst_LIBRARIES = libhardware.a

libhardware_a_SOURCES = adlib.cpp dma.cpp gameblaster.cpp hardware.cpp iohandler.cpp joystick.cpp keyboard.cpp \
                        memory.cpp mixer.cpp pcspeaker.cpp pci_bus.cpp pic.cpp sblaster.cpp tandy_sound.cpp timer.cpp \
			vga.cpp vga_attr.cpp vga_crtc.cpp vga_dac.cpp vga_draw.cpp vga_gfx.cpp vga_other.cpp \
			vga_memory.cpp vga_misc.cpp vga_seq.cpp vga_xga.cpp vga_s3.cpp vga_tseng.cpp vga_paradise.cpp \
			cmos.cpp disney.cpp gus.cpp mpu401.cpp ipx.cpp ipxserver.cpp ne2000.cpp hardopl.cpp dbopl.cpp innova.cpp dongle.cpp \
			voodoo.cpp voodoo_interface.cpp voodoo_emu.cpp ps1_sound.cpp sn76496.h ide.cpp floppy.cpp voodoo_vogl.cpp voodoo_opengl.cpp nukedopl.cpp pc98.cpp vga_pc98_gdc.cpp vga_pc98_dac.cpp vga_pc98_crtc.cpp vga_pc98_cg.cpp vga_pc98_egc.cpp pc98_fm.cpp glide.cpp \
			snd_pc98/sound/opngenc.c snd_pc98/sound/opngeng.c snd_pc98/sound/pcm86c.c snd_pc98/sound/pcm86g.c \
			snd_pc98/sound/tms3631c.c snd_pc98/sound/tms3631g.c snd_pc98/sound/psggenc.c snd_pc98/sound/psggeng.c \
			snd_pc98/common/parts.c snd_pc98/generic/keydisp.c snd_pc98/sound/adpcmc.c snd_pc98/sound/adpcmg.c \
			snd_pc98/sound/rhythmc.c snd_pc98/sound/sound.c snd_pc98/sound/getsnd/getwave.c snd_pc98/sound/getsnd/getsmix.c \
			snd_pc98/sound/getsnd/getsnd.c snd_pc98/x11/dosio.c snd_pc98/sound/fmboard.c snd_pc98/sound/soundrom.c \
			snd_pc98/cbus/board86.c snd_pc98/sound/fmtimer.c snd_pc98/cbus/board26k.c 8255.cpp opl2board/opl2board.cpp opl3duoboard/opl3duoboard.cpp

