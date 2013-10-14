#define VERSION "SVN-Daum"

/* Define to 1 to enable internal debugger, requires libcurses */
#define C_DEBUG 0

/* Define to 1 to enable screenshots, requires libpng */
#define C_SSHOT 1

/* Define to 1 to use opengl display output support */
#define C_OPENGL 1

/* Define to 1 to enable internal modem support, requires SDL_net */
#define C_MODEM 1

/* Define to 1 to enable IPX networking support, requires SDL_net */
#define C_IPX 1

/* Enable some heavy debugging options */
#define C_HEAVY_DEBUG 0

/* The type of cpu this host has */
#define C_TARGETCPU X86
//#define C_TARGETCPU X86_64

/* Define to 1 to use x86 dynamic cpu core */
#define C_DYNAMIC_X86 1

/* Define to 1 to use recompiling cpu core. Can not be used together with the dynamic-x86 core */
#define C_DYNREC 0

/* Enable memory function inlining in */
#define C_CORE_INLINE 1

/* Enable the FPU module, still only for beta testing */
#define C_FPU 1

/* Define to 1 to use a x86 assembly fpu core */
#define C_FPU_X86 1

/* Define to 1 to use a unaligned memory access */
#define C_UNALIGNED_MEMORY 1

/* environ is defined */
#define ENVIRON_INCLUDED 1

/* environ can be linked */
#define ENVIRON_LINKED 1

/* Define to 1 if you have the <ddraw.h> header file. */
#define HAVE_DDRAW_H 1

/* Define to 1 if you have the <d3d9.h> header file. */
#define HAVE_D3D9_H 1

/* Define to 1 to use Direct3D shaders, requires d3d9.h and libd3dx9 */
#define C_D3DSHADERS 1

/* Define to 1 if you want serial passthrough support (Win32 only). */
#define C_DIRECTSERIAL 1

/* My defines */
#define C_LIBPNG 1
#define C_PRINTER 1
#define C_NE2000 1
#define C_DIRECTLPT 1
#define C_HAVE_PHYSFS 1
#define C_FLUIDSYNTH 1
#define C_SDL_SOUND 1
#define __WIN32__ 1
#define WIN32 1
#define __SSE__ 1

#define GCC_ATTRIBUTE(x) /* attribute not supported */
#define GCC_UNLIKELY(x) (x)
#define GCC_LIKELY(x) (x)

#define INLINE __forceinline
#define DB_FASTCALL __fastcall

#if defined(_MSC_VER) && (_MSC_VER >= 1400) 
#pragma warning(disable : 4996) 
#endif

typedef         double		Real64;
/* The internal types */
typedef  unsigned char		Bit8u;
typedef    signed char		Bit8s;
typedef unsigned short		Bit16u;
typedef   signed short		Bit16s;
typedef  unsigned long		Bit32u;
typedef    signed long		Bit32s;
typedef unsigned __int64	Bit64u;
typedef   signed __int64	Bit64s;
typedef unsigned int		Bitu;
typedef signed int			Bits;

