AM_CPPFLAGS = -I$(top_srcdir)/include

noinst_LIBRARIES = libdecoders.a

libdecoders_a_SOURCES = \
	archive.h            \
	dr_flac.h            \
	dr_mp3.h             \
	dr_wav.h             \
	flac.c               \
	mp3.cpp              \
	mp3_seek_table.cpp   \
	mp3_seek_table.h     \
	SDL_sound.c          \
	SDL_sound.h          \
	SDL_sound_internal.h \
	stb.h                \
	stb_vorbis.h         \
	vorbis.c             \
	wav.c                \
	xxh3.h               \
	xxhash.h

libdecoders_a_CXXFLAGS = \
	$(AM_CXXFLAGS) \
	$(CXXFLAGS) \
	-Wpedantic \
	-Wall

libdecoders_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(CFLAGS) \
	-Wpedantic \
	-Wall

if USE_OPUS
libdecoders_a_SOURCES += opus.cpp
libdecoders_a_CFLAGS += -DUSE_OPUS # Ensures Opus is registered in SDL_sound.c
libdecoders_a_CXXFLAGS += $(OPUSFILE_CFLAGS) # Ensures opus.cpp find the header
endif
