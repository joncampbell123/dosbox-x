# makefile for mingw SDL2
#   gcc version 6.3

CC		=	gcc
CXX		=   g++
STRIP	=	strip
RM		=	rm -f

DBX_PATH	= ../..

MT32 = 1

CFLAGS	=	-O3 -DNDEBUG -Wstrict-overflow=0 -Wstrict-aliasing=0 -fomit-frame-pointer  -mno-ms-bitfields  -I. \
			-I$(DBX_PATH) \
			-I$(DBX_PATH)/include \
			-I$(DBX_PATH)/src \
			-I$(DBX_PATH)/src/aviwriter \
			-I$(DBX_PATH)/src/libs/xBRZ \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound \
			-I$(DBX_PATH)/src/hardware/snd_pc98/common \
			-I$(DBX_PATH)/src/hardware/snd_pc98/generic \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound/getsnd \
			-I$(DBX_PATH)/src/hardware/snd_pc98/x11 \
			-I$(DBX_PATH)/src/hardware/snd_pc98/cbus 
			
CFLAGS	+=	$(shell sdl2-config --cflags)

CXXFLAGS = -O3  -DNDEBUG -std=gnu++11 -Wstrict-overflow=0 -Wstrict-aliasing=0 -fomit-frame-pointer  -mno-ms-bitfields  -I. \
			-I$(DBX_PATH) \
			-I$(DBX_PATH)/include \
			-I$(DBX_PATH)/src \
			-I$(DBX_PATH)/src/aviwriter \
			-I$(DBX_PATH)/src/libs/xBRZ \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound \
			-I$(DBX_PATH)/src/hardware/snd_pc98/common \
			-I$(DBX_PATH)/src/hardware/snd_pc98/generic \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound/getsnd \
			-I$(DBX_PATH)/src/hardware/snd_pc98/x11 \
			-I$(DBX_PATH)/src/hardware/snd_pc98/cbus 
			
CXXFLAGS	+=	$(shell sdl2-config --cflags)

LFLAGS	=

TARGET	=	dosbox-x.exe

CPPSRCS	=	$(DBX_PATH)/src/dosbox.cpp \
			$(wildcard $(DBX_PATH)/src/aviwriter/*.cpp) \
			$(wildcard $(DBX_PATH)/src/builtin/*.cpp) \
			$(wildcard $(DBX_PATH)/src/cpu/*.cpp) \
			$(wildcard $(DBX_PATH)/src/debug/*.cpp) \
			$(wildcard $(DBX_PATH)/src/dos/*.cpp) \
			$(wildcard $(DBX_PATH)/src/fpu/*.cpp) \
			$(wildcard $(DBX_PATH)/src/gui/*.cpp) \
			$(wildcard $(DBX_PATH)/src/gui/*.c) \
			$(wildcard $(DBX_PATH)/src/hardware/*.cpp) \
			$(wildcard $(DBX_PATH)/src/hardware/opl2board/*.cpp) \
			$(wildcard $(DBX_PATH)/src/hardware/mame/*.cpp) \
			$(wildcard $(DBX_PATH)/src/hardware/parport/*.cpp) \
			$(wildcard $(DBX_PATH)/src/hardware/reSID/*.cpp) \
			$(wildcard $(DBX_PATH)/src/hardware/serialport/*.cpp) \
			$(DBX_PATH)/src/hardware/snd_pc98/cbus/board86.c \
			$(DBX_PATH)/src/hardware/snd_pc98/cbus/board26k.c \
			$(wildcard $(DBX_PATH)/src/hardware/snd_pc98/common/*.c) \
			$(wildcard $(DBX_PATH)/src/hardware/snd_pc98/generic/*.c) \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/opngeng.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/opngenc.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/pcm86g.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/pcm86c.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/tms3631g.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/tms3631c.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/psggeng.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/psggenc.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/adpcmg.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/adpcmc.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/rhythmc.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/fmboard.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/fmtimer.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/sound.c \
			$(DBX_PATH)/src/hardware/snd_pc98/sound/soundrom.c \
			$(wildcard $(DBX_PATH)/src/hardware/snd_pc98/sound/getsnd/*.c) \
			$(wildcard $(DBX_PATH)/src/hardware/snd_pc98/x11/*.c) \
			$(wildcard $(DBX_PATH)/src/ints/*.cpp) \
			$(wildcard $(DBX_PATH)/src/libs/gui_tk/*.cpp) \
			$(wildcard $(DBX_PATH)/src/libs/zmbv/*.cpp) \
			$(wildcard $(DBX_PATH)/src/misc/*.cpp) \
			$(wildcard $(DBX_PATH)/src/output/*.cpp) \
			$(wildcard $(DBX_PATH)/src/output/direct3d/*.cpp) \
			$(wildcard $(DBX_PATH)/src/shell/*.cpp) \
			$(wildcard $(DBX_PATH)/src/libs/xBRZ/*.cpp)

CPPSRCS_MT32 = $(wildcard $(DBX_PATH)/src/libs/mt32/*.cpp) \
			$(wildcard $(DBX_PATH)/src/libs/mt32/c_interface/*.cpp) \
			$(wildcard $(DBX_PATH)/src/libs/mt32/sha1/*.cpp)

OBJS_MT32 = $(addsuffix .o,$(basename $(CPPSRCS_MT32)))

CXXFLAGS_MT32 = -DC_MT32=1 -I$(DBX_PATH)/src/libs/mt32 -I$(DBX_PATH)/src/libs/mt32/freeverb -I$(DBX_PATH)/src/libs/mt32/sha1

OBJS = $(addsuffix .o,$(basename $(CPPSRCS)))
LIBS = -static -lpng -lz -lm -lstdc++ $(shell sdl2-config --static-libs)

ifeq ($(OPENGL), 1)
	CXXFLAGS += -DC_OPENGL=1
	ifdef WINDIR
		LIBS += -lopengl32
	else
		LIBS += -lGL
	endif
endif

ifeq ($(MT32),1)
	CPPSRCS += $(CPPSRCS_MT32)
	CXXFLAGS += $(CXXFLAGS_MT32)
#	OBJS += $(OBJS_MT32)
endif

.SUFFIXES: .c.o
.SUFFIXES: .cpp.o

all:	$(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(LFLAGS) -g -o $@ $(OBJS) $(LIBS)
	$(STRIP) $@

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) $(OBJS_MT32)
