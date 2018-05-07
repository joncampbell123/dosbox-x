AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/aviwriter "-DRESDIR=\"$(resdir)\""

noinst_LIBRARIES = libgui.a
libgui_a_SOURCES = \
	sdlmain_linux.cpp \
	sdlmain.cpp sdl_mapper.cpp dosbox_logo.h \
	render.cpp render_scalers.cpp render_scalers.h \
	render_templates.h render_loops.h render_simple.h \
	render_templates_sai.h render_templates_hq.h \
	render_templates_hq2x.h render_templates_hq3x.h \
	midi.cpp midi_win32.h midi_oss.h midi_coreaudio.h midi_alsa.h \
	direct3d.cpp direct3d.h \
	hq2x_d3d.cpp hq2x_d3d.h \
	midi_coremidi.h sdl_gui.cpp dosbox_splash.h menu.cpp

if C_MT32
AM_CPPFLAGS += -I$(top_srcdir)/src/mt32

libgui_a_SOURCES += \
	midi_mt32.h
endif

if MACOSX
libgui_a_SOURCES += \
	menu_osx.mm
endif

