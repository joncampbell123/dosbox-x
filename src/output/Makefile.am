AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src

SUBDIRS =

if C_DIRECT3D
SUBDIRS += direct3d
endif

noinst_LIBRARIES = liboutput.a
liboutput_a_SOURCES = output_direct3d.cpp output_opengl.cpp output_surface.cpp output_tools.cpp output_tools_xbrz.cpp output_ttf.cpp

if C_GAMELINK
liboutput_a_SOURCES += output_gamelink.cpp
endif
