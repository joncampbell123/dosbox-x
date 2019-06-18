#ifndef DOSBOX_EMU_H
#define DOSBOX_EMU_H


#include "dosbox.h"
#if defined(_MSC_VER) && (_MSC_VER  <= 1500) 
#include <SDL.h>
#else
#include <stdint.h>
#endif
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <memory.h>

#if C_DEBUG
#include <stdio.h>
#include <stdarg.h>
#endif

#ifndef M_PI
#define M_PI           3.14159265358979323846
#endif

typedef Bit16s stream_sample_t;

typedef Bit8u u8;
typedef Bit32u u32;

class device_t;
struct machine_config;

#define NAME( _ASDF_ ) 0
#define BIT( _INPUT_, _BIT_ ) ( ( _INPUT_) >> (_BIT_)) & 1

#define ATTR_UNUSED
#define DECLARE_READ8_MEMBER(name)      u8     name( int, int)
#define DECLARE_WRITE8_MEMBER(name)     void   name( int, int, u8 data)
#define READ8_MEMBER(name)              u8     name( int, int)
#define WRITE8_MEMBER(name)				void   name( int offset, int space, u8 data)

#define DECLARE_DEVICE_TYPE(Type, Class) \
		extern const device_type Type; \
		class Class;

#define DEFINE_DEVICE_TYPE(Type, Class, ShortName, FullName)		\
	const device_type Type = 0;


class device_sound_interface {
public:			
	struct sound_stream {
		void update() {
		}
	};
	sound_stream temp;

	sound_stream* stream_alloc(int whatever, int channels, int size) {
		return &temp;
	};
	

	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) = 0;

	device_sound_interface(const machine_config &mconfig, device_t& _device) {
	}

};

struct attotime {
	int whatever;

	static attotime from_hz(int hz) {
		return attotime();
	}
};

struct machine_config {
};

typedef int device_type;

class device_t {
	u32 clockRate;
public:
	struct machine_t {
		int describe_context() const {
			return 0;
		}
	};

	machine_t machine() const {
		return machine_t();
	}

	//int offset, space;

	u32 clock() const {
		return clockRate;
	}

	void logerror(const char* format, ...) {
#if C_DEBUG
		char buf[512*2];
		va_list msg;
		va_start(msg,format);
		vsprintf(buf,format,msg);
		va_end(msg);
		LOG(LOG_MISC,LOG_NORMAL)("%s",buf);
#endif
	}

	static int tag() {
		return 0;
	}

	virtual void device_start() {
	}

	void save_item(int wtf, int blah= 0) {
	}

	device_t(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 _clock) : clockRate( _clock ) {
	}

};



static void auto_free(const device_t::machine_t& machine, void * buffer) {
	free(buffer);
}

#define auto_alloc_array_clear(m, t, c) calloc(c, sizeof(t) )
#define auto_alloc_clear(m, t) static_cast<t*>( calloc(1, sizeof(t) ) )




#endif
