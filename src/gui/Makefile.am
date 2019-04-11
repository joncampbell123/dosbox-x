AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src/aviwriter "-DRESDIR=\"$(resdir)\"" -I$(top_srcdir)/src

noinst_LIBRARIES = libgui.a
libgui_a_SOURCES = \
	sdlmain_linux.cpp \
	sdlmain.cpp sdl_mapper.cpp dosbox_logo.h \
	render.cpp render_scalers.cpp render_scalers.h \
	render_templates.h render_loops.h render_simple.h \
	render_templates_sai.h render_templates_hq.h \
	render_templates_hq2x.h render_templates_hq3x.h \
	sdl_gui.cpp menu.cpp bitop.cpp ptrop.cpp zipcrc.c zipfile.cpp

if MACOSX
libgui_a_SOURCES += \
	menu_osx.mm
endif

