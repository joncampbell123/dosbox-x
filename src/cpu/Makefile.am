SUBDIRS = core_full core_normal core_dyn_x86 core_dynrec
AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libcpu.a
libcpu_a_SOURCES = callback.cpp cpu.cpp flags.cpp modrm.cpp modrm.h core_full.cpp instructions.h	\
		   paging.cpp lazyflags.h core_normal.cpp core_simple.cpp core_prefetch.cpp \
		   core_dyn_x86.cpp core_dynrec.cpp mmx.cpp
