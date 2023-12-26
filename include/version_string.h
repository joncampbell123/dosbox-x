#if WIN32 && !defined(HX_DOS)
#ifdef _MSC_VER
#define OS_PLATFORM "Windows"
#elif defined(__MINGW32__)
#define OS_PLATFORM "MinGW"
#else
#define OS_PLATFORM "Windows"
#endif
#elif defined(HX_DOS)
#define OS_PLATFORM "DOS"
#elif defined(LINUX)
#define OS_PLATFORM "Linux"
#elif defined(MACOSX)
#define OS_PLATFORM "macOS"
#else
#define OS_PLATFORM ""
#endif

#if defined(_M_X64) || defined (_M_AMD64) || defined (_M_ARM64) || defined (_M_IA64) || defined(__ia64__) || defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__powerpc64__)
#define OS_BIT "64"
#define OS_BIT_INT 64
#else
#define OS_BIT "32"
#define OS_BIT_INT 32
#endif