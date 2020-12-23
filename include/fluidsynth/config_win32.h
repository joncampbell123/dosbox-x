#define FLUIDSYNTHVERSION "@VERSION@"

#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_MATH_H 1
#define HAVE_STDARG_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_IO_H 1
#define HAVE_WINDOWS_H 1

#define WITHOUT_SERVER 1
#define SDL2_SUPPORT 1
#define DSOUND_SUPPORT 1
#define WINMIDI_SUPPORT 0
#define AUFILE_SUPPORT 0
#define WITH_FLOAT 1

/* MinGW32 special defines */
#if defined(__MINGW32__)
#include <stdint.h>
#if !defined(__MINGW64_VERSION_MAJOR)
#define DSOUND_SUPPORT 0
#endif
//#define snprintf _snprintf
//#define vsnprintf _vsnprintf
#else
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif
typedef int socklen_t;
#endif

#if _MSC_VER
#define alloca _alloca
#endif

#define strcasecmp _stricmp


#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


#define WITH_PROFILING 0

//#pragma warning(disable : 4244)
//#pragma warning(disable : 4101)
//#pragma warning(disable : 4305)
#pragma warning(disable : 4996)

#ifndef inline
#define inline __inline
#endif

#define DEFAULT_SOUNDFONT "generalmidi.sf2"
