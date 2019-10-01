# makefile for Emscripten SDL2
# Emscripten version 1.38.12, clang version 6.0.1

CC		=	emcc
CXX		=   em++
RM		=	rm -f

ifeq ($(EMSCRIPTEN_ROOT),)
#Set it yourself if EMSCRIPTEN_ROOT not there
	EMSCRIPTEN_ROOT=c:/emsdk/emscripten/1.38.12
endif

#WebAssembly seemed to be a bit faster than asm.js
WASM=1

#Set maximum RAM that Emscripten use (in bytes).
EMSCRIPTEN_TOTAL_MEMORY=83886080

SDL_CONFIG ?= $(EMSCRIPTEN_ROOT)/system/bin/sdl2-config 
SDL_CFLAGS := $(shell $(SDL_CONFIG) --cflags) 
SDL_LIBS := $(shell $(SDL_CONFIG) --libs)

DBX_PATH	= ../..

CFLAGS	=	-O3 -DNDEBUG -DEMTERPRETER_SYNC -Wstrict-overflow=0 -Wstrict-aliasing=0 -I. \
			-I$(DBX_PATH) \
			-I$(DBX_PATH)/include \
			-I$(DBX_PATH)/src \
			-I$(DBX_PATH)/src/aviwriter \
			-I$(DBX_PATH)/src/xBRZ \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound \
			-I$(DBX_PATH)/src/hardware/snd_pc98/common \
			-I$(DBX_PATH)/src/hardware/snd_pc98/generic \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound/getsnd \
			-I$(DBX_PATH)/src/hardware/snd_pc98/x11 \
			-I$(DBX_PATH)/src/hardware/snd_pc98/cbus 
			
CFLAGS	+=	$(SDL_CFLAGS)

CXXFLAGS = -O3 -DNDEBUG -DEMTERPRETER_SYNC -std=gnu++11 -Wstrict-overflow=0 -Wstrict-aliasing=0 -I. \
			-I$(DBX_PATH) \
			-I$(DBX_PATH)/include \
			-I$(DBX_PATH)/src \
			-I$(DBX_PATH)/src/aviwriter \
			-I$(DBX_PATH)/src/xBRZ \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound \
			-I$(DBX_PATH)/src/hardware/snd_pc98/common \
			-I$(DBX_PATH)/src/hardware/snd_pc98/generic \
			-I$(DBX_PATH)/src/hardware/snd_pc98/sound/getsnd \
			-I$(DBX_PATH)/src/hardware/snd_pc98/x11 \
			-I$(DBX_PATH)/src/hardware/snd_pc98/cbus 
			
CXXFLAGS	+=	$(SDL_CFLAGS)
	
LFLAGS	=

TARGET	=	dosbox-x.bc

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
			$(wildcard $(DBX_PATH)/src/misc/*.cpp) \
			$(wildcard $(DBX_PATH)/src/output/*.cpp) \
			$(wildcard $(DBX_PATH)/src/output/direct3d/*.cpp) \
			$(wildcard $(DBX_PATH)/src/shell/*.cpp) \
			$(wildcard $(DBX_PATH)/src/xBRZ/*.cpp)

OBJS = $(addsuffix .o,$(basename $(CPPSRCS)))
LIBS = -lm  $(SDL_LIBS) 

.SUFFIXES: .c.o
.SUFFIXES: .cpp.o

all:	$(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(LFLAGS) -g -o $@ $(OBJS) $(LIBS)
	
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

.cpp.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

html: $(TARGET)
ifeq ($(PRELOAD), 1)
	$(CC) -O3 -s USE_SDL=2 -s WASM=$(WASM) -s TOTAL_MEMORY=$(EMSCRIPTEN_TOTAL_MEMORY)  -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1 \
	$(TARGET) \
	--preload-file $(PREFILE) -o $(basename $(TARGET)).html 
else
	$(CC) -O3 -s USE_SDL=2 -s WASM=$(WASM) -s TOTAL_MEMORY=$(EMSCRIPTEN_TOTAL_MEMORY)  -s EMTERPRETIFY=1 -s EMTERPRETIFY_ASYNC=1 \
	$(TARGET) -o $(basename $(TARGET)).html 
endif
#TODO: make an EMTERPRETIFY_WHITELIST file.