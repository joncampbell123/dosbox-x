SUBDIRS = core_normal
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src

noinst_LIBRARIES = libcpu.a
libcpu_a_SOURCES = callback.cpp cpu.cpp flags.cpp modrm.cpp modrm.h instructions.h	\
		   paging.cpp lazyflags.h core_normal.cpp core_prefetch.cpp \
		   mmx.cpp core_prefetch_286.cpp

