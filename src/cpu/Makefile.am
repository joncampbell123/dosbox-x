SUBDIRS = core_full core_normal
AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src

noinst_LIBRARIES = libcpu.a
libcpu_a_SOURCES = callback.cpp cpu.cpp flags.cpp modrm.cpp modrm.h instructions.h	\
		   paging.cpp lazyflags.h core_normal.cpp core_normal_8086.cpp core_normal_286.cpp core_prefetch.cpp \
		   core_dyn_x86.cpp core_dynrec.cpp mmx.cpp core_prefetch_286.cpp core_prefetch_8086.cpp

if !EMSCRIPTEN
libcpu_a_SOURCES += core_full.cpp core_simple.cpp
endif

