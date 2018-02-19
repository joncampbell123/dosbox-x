SUBDIRS = core_full core_normal
AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcpu.a
libcpu_a_SOURCES = callback.cpp cpu.cpp flags.cpp modrm.cpp modrm.h core_full.cpp instructions.h	\
		   paging.cpp lazyflags.h core_normal.cpp core_normal_8086.cpp core_normal_286.cpp core_simple.cpp core_prefetch.cpp \
		   mmx.cpp
