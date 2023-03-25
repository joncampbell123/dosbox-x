#include "passthroughio.h"
#include "logging.h"

// headers necessary for Visual C++, but perhaps also for other (non-GCC,
// non-Clang) Windows compilers
#if (defined _M_IX86 || defined _M_X64) && defined _WIN32
#if defined _MSC_VER && _MSC_VER >= 1900        // Visual Studio 2015 or later
#include <intrin.h>                             // __in{byte, word}() & __out{byte, word}()
#else
#include <conio.h>                              // inp{w}() & outp{w}()
#endif
#endif // x86{_64} & Windows

static inline uint8_t x86_input_byte(uint16_t port) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	uint8_t value;
	__asm__ __volatile__ (
		"inb %1, %0"
		: "=a" (value)
		: "d" (port)
	);
	return value;
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	return __inbyte(port);
#else
	return (uint8_t)inp(port);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	return 0;
#endif
}

static inline void x86_output_byte(uint16_t port, uint8_t value) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	__asm__ __volatile__ (
		"outb %1, %0"
		:
		: "d" (port), "a" (value)
	);
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	__outbyte(port, value);
#else
	outp(port, value);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	(void)value;
#endif
}

static inline uint16_t x86_input_word(uint16_t port) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	uint16_t value;
	__asm__ __volatile__ (
		"inw %1, %0"
		: "=a" (value)
		: "d" (port)
	);
	return value;
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	return __inword(port);
#else
	return inpw(port);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	return 0;
#endif
}

static inline void x86_output_word(uint16_t port, uint16_t value) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	__asm__ __volatile__ (
		"outw %1, %0"
		:
		: "d" (port), "a" (value)
	);
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	__outword(port, value);
#else
	outpw(port, value);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	(void)value;
#endif
}

static inline uint32_t x86_input_dword(uint16_t port) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	uint32_t value;
	__asm__ __volatile__ (
		"inl %1, %0"
		: "=a" (value)
		: "d" (port)
	);
	return value;
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	return __indword(port);
#else
	return _inpd(port);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	return 0;
#endif
}

static inline void x86_output_dword(uint16_t port, uint32_t value) {
#if defined __i386__ || defined __x86_64__      // GCC or Clang
	__asm__ __volatile__ (
		"outl %1, %0"
		:
		: "d" (port), "a" (value)
	);
#elif defined _MSC_VER && (defined _M_IX86 || defined _M_X64)
#if _MSC_VER >= 1900                            // Visual Studio 2015 or later
	__outdword(port, value);
#else
	_outpd(port, value);
#endif // _MSC_VER >= 1900
#else // _MSC_VER
	(void)port;
	(void)value;
#endif
}

static uint8_t (*input_byte)(uint16_t) = x86_input_byte;
static void (*output_byte)(uint16_t, uint8_t) = x86_output_byte;

static uint16_t (*input_word)(uint16_t) = x86_input_word;
static void (*output_word)(uint16_t, uint16_t) = x86_output_word;

static uint32_t (*input_dword)(uint16_t) = x86_input_dword;
static void (*output_dword)(uint16_t, uint32_t) = x86_output_dword;

uint8_t inportb(uint16_t port) { return input_byte(port); }
void outportb(uint16_t port, uint8_t value) { output_byte(port, value); }

uint16_t inportw(uint16_t port) { return input_word(port); }
void outportw(uint16_t port, uint16_t value) { output_word(port, value); }

uint32_t inportd(uint16_t port) { return input_dword(port); }
void outportd(uint16_t port, uint32_t value) { output_dword(port, value); }

#if ((defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
     (defined _WIN32 || defined BSD || defined LINUX || defined __CYGWIN__)) // _WIN32 is not defined by default on Cygwin
static bool passthroughIO_enabled = false;      // must default to false for UNIX version
#endif

/*
  For MinGW and MinGW-w64, _M_IX86 and _M_X64 are not defined by the compiler,
  but in a header file, which has to be (indirectly) included, usually through a
  C (not C++) standard header file. For MinGW it is sdkddkver.h and for
  MinGW-w64 it is _mingw_mac.h. Do not rely on constants that may not be
  defined, depending on what was included before these lines.
*/
#if ((defined __i386__ || defined __x86_64__ || defined _M_IX86 || defined _M_X64) && \
     (defined _WIN32 || defined __CYGWIN__))    // _WIN32 is not defined by default on Cygwin
#include <io.h>
#include <string.h>
#include <windows.h>
#ifdef __CYGWIN__
#include <errno.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#endif

#define TESTIOPORT (0x378 + 0x402)

static HMODULE io_driver = NULL;

/*
  The original DlPortIO uses ULONG for the port argument (which is plain wrong,
  because I/O port addresses are 16-bit). I define the prototypes just as
  InpOut32/InpOutx64 does, which correctly uses USHORT, but unfortunately uses
  ULONG for the 32-bit I/O functions.
*/
static UCHAR (__stdcall *DlPortReadPortUchar)(USHORT) = NULL;
static void (__stdcall *DlPortWritePortUchar)(USHORT, UCHAR) = NULL;

static USHORT (__stdcall *DlPortReadPortUshort)(USHORT) = NULL;
static void (__stdcall *DlPortWritePortUshort)(USHORT, USHORT) = NULL;

static ULONG (__stdcall *DlPortReadPortUlong)(ULONG) = NULL;
static void (__stdcall *DlPortWritePortUlong)(ULONG, ULONG) = NULL;

static uint8_t dlportio_input_byte(uint16_t port) { return DlPortReadPortUchar(port); }
static void dlportio_output_byte(uint16_t port, uint8_t value) { DlPortWritePortUchar(port, value); }

static uint16_t dlportio_input_word(uint16_t port) { return DlPortReadPortUshort(port); }
static void dlportio_output_word(uint16_t port, uint16_t value) { DlPortWritePortUshort(port, value); }

static uint32_t dlportio_input_dword(uint16_t port) { return DlPortReadPortUlong(port); }
static void dlportio_output_dword(uint16_t port, uint32_t value) { DlPortWritePortUlong(port, value); }

#ifdef __CYGWIN__
static void ioSignalHandler(int signum, siginfo_t* info, void* ctx) {
	if(signum == SIGILL && info->si_code == ILL_PRVOPC &&
		*(uint8_t*)info->si_addr == 0xec) { // in al, dx
		ucontext_t* ucontext = (ucontext_t *) ctx, ucontext2;
#ifdef __x86_64__
		ucontext->uc_mcontext.rip++;            // skip 1 byte in instruction
#else
		ucontext->uc_mcontext.eip++;            // skip 1 byte in instruction
#endif
		passthroughIO_enabled = false;

		/*
		  Both the 32-bit and the 64-bit Cygwin I use pass a ucontext_t with
		  an invalid value in ucontext_t.uc_mcontext.ctxflags, which causes
		  setcontext() to fail. I fix this by attempting to obtain a valid
		  value from getcontext().
		*/
		if(getcontext(&ucontext2) == -1)
			LOG_MSG("Error: getcontext() failed: %s", strerror(errno));
		else
			ucontext->uc_mcontext.ctxflags = ucontext2.uc_mcontext.ctxflags;
	}
}
#else
static LONG WINAPI ioExceptionFilter(LPEXCEPTION_POINTERS exception_pointers) {
	if(exception_pointers->ExceptionRecord->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION &&
#ifdef _M_X64                                   // also defined for MinGW-w64
		*(uint8_t*)exception_pointers->ContextRecord->Rip == 0xec) { // in al, dx
		exception_pointers->ContextRecord->Rip++; // skip 1 byte in instruction
#else
		*(uint8_t*)exception_pointers->ContextRecord->Eip == 0xec) { // in al, dx
		exception_pointers->ContextRecord->Eip++; // skip 1 byte in instruction
#endif
		passthroughIO_enabled = false;
		return EXCEPTION_CONTINUE_EXECUTION;    // continue at ContextRecord->{E, R}ip
	}
	return EXCEPTION_CONTINUE_SEARCH;           // in practice, crash the application
}
#endif

#ifndef F_OK                                    // necessary for VC++
#define F_OK 00
#endif

static bool loadIODriver(const char* filename, HMODULE& driver_handle) {
	if(access(filename, F_OK) != 0)
		return false;

	if((driver_handle = LoadLibrary(filename)) == NULL) {
		LPTSTR strptr;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		              FORMAT_MESSAGE_FROM_SYSTEM |
		              FORMAT_MESSAGE_IGNORE_INSERTS,
		              NULL, GetLastError(),
		              MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		              (LPTSTR)&strptr, 0, NULL);
		// Note the construct with strptr.
		char buffer[160];
		strncpy(buffer, strptr, sizeof buffer - 1)[sizeof buffer - 1] = '\0';
		LocalFree(strptr);
		LOG_MSG("Error: LoadLibrary(): %s", buffer);
		return false;
	}

	return true;
}

bool initPassthroughIO(void) {
	// With giveio64 running, direct I/O will not cause an exception.
	passthroughIO_enabled = true;
#ifdef __CYGWIN__                               // Cygwin
	struct sigaction new_sigaction;
	memset(&new_sigaction, 0, sizeof new_sigaction);
	new_sigaction.sa_flags = SA_SIGINFO | SA_RESETHAND;
	new_sigaction.sa_sigaction = ioSignalHandler;

	struct sigaction org_sigaction;
	if(sigaction(SIGILL, &new_sigaction, &org_sigaction) == -1)
		LOG_MSG("Error: sigaction() failed: %s", strerror(errno));
	input_byte(TESTIOPORT);
	if(sigaction(SIGILL, &org_sigaction, NULL) == -1)
		LOG_MSG("Error: sigaction() failed: %s", strerror(errno));
#elif defined _WIN32                            // MinGW & Visual C++
	LPTOP_LEVEL_EXCEPTION_FILTER org_exception_filter =
		SetUnhandledExceptionFilter(ioExceptionFilter);
	input_byte(TESTIOPORT);
	SetUnhandledExceptionFilter(org_exception_filter);
#endif

	if(!passthroughIO_enabled) {                // We may be called more than once.
#if defined __x86_64__ || defined _M_X64
		LOG_MSG("Pass-through I/O caused exception. Right driver required (inpout32.dll or inpoutx64.dll).");
#else
		LOG_MSG("Pass-through I/O caused exception. Right driver required (inpout32.dll).");
#endif

		// if non-null a previous call to this function must have failed
		if(io_driver == NULL) {
			const char* io_driver_filename = "inpout32.dll";
			if(!loadIODriver(io_driver_filename, io_driver)) {
#if defined __x86_64__ || defined _M_X64
				io_driver_filename = "inpoutx64.dll";
				if(!loadIODriver(io_driver_filename, io_driver)) {
#endif
					LOG_MSG("Error: Could not load driver.");
					return false;
#if defined __x86_64__ || defined _M_X64
				}
#endif
			}

			/*
			  GetProcAddress() returns a FARPROC value, which is
			  int (__attribute__((stdcall)) *)(), so we should cast to the right
			  type. Visual Studio 2022 (with /Wall) cannot be shut up and
			  neither can GCC 8+ (with -Wextra), directly, without specifically
			  turning the warning off.
			*/
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4191) // 'operator/operation' : unsafe conversion from 'type of expression' to 'type required'
#endif
#if defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
			/*
			  Later ports of inpout32.dll/inpoutx64.dll also contain the API
			  provided by dlportio.dll. Since the API of dlportio.dll does not
			  have the flaws of the original inpout32.dll (*signed* short return
			  value and arguments), and it has functions for 16-bit and 32-bit
			  I/O, we prefer it.
			*/
			DlPortReadPortUchar = (UCHAR(__stdcall *)(USHORT))GetProcAddress(io_driver, "DlPortReadPortUchar");
			if(DlPortReadPortUchar == NULL) return false;
			input_byte = dlportio_input_byte;

			DlPortWritePortUchar = (void(__stdcall *)(USHORT, UCHAR))GetProcAddress(io_driver, "DlPortWritePortUchar");
			if(DlPortWritePortUchar == NULL) return false;
			output_byte = dlportio_output_byte;

			DlPortReadPortUshort = (USHORT(__stdcall *)(USHORT))GetProcAddress(io_driver, "DlPortReadPortUshort");
			if(DlPortReadPortUshort == NULL) return false;
			input_word = dlportio_input_word;

			DlPortWritePortUshort = (void(__stdcall *)(USHORT, USHORT))GetProcAddress(io_driver, "DlPortWritePortUshort");
			if(DlPortWritePortUshort == NULL) return false;
			output_word = dlportio_output_word;

			DlPortReadPortUlong = (ULONG(__stdcall *)(ULONG))GetProcAddress(io_driver, "DlPortReadPortUlong");
			if(DlPortReadPortUlong == NULL) return false;
			input_dword = dlportio_input_dword;

			DlPortWritePortUlong = (void(__stdcall *)(ULONG, ULONG))GetProcAddress(io_driver, "DlPortWritePortUlong");
			if(DlPortWritePortUlong == NULL) return false;
			output_dword = dlportio_output_dword;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#if defined __GNUC__ && __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif

			passthroughIO_enabled = true;
#ifdef __CYGWIN__                               // Cygwin
			if(sigaction(SIGILL, &new_sigaction, &org_sigaction) == -1)
				LOG_MSG("Error: sigaction() failed: %s", strerror(errno));
			input_byte(TESTIOPORT);
			if(sigaction(SIGILL, &org_sigaction, NULL) == -1)
				LOG_MSG("Error: sigaction() failed: %s", strerror(errno));
#elif defined _WIN32                            // MinGW & Visual C++
			org_exception_filter = SetUnhandledExceptionFilter(ioExceptionFilter);
			input_byte(TESTIOPORT);
			SetUnhandledExceptionFilter(org_exception_filter);
#endif

			if(!passthroughIO_enabled) return false;
			LOG_MSG("Using driver %s.", io_driver_filename);
		}
	}
	return passthroughIO_enabled;
}
#endif // x86{_64} & Windows

#if (defined __i386__ || defined __x86_64__) && (defined BSD || defined LINUX)
#include <unistd.h>
#ifdef LINUX
#ifndef __ANDROID__
#include <sys/io.h>
#endif
#include <sys/types.h>
#elif defined __OpenBSD__ || defined __NetBSD__
#include <machine/sysarch.h>
#include <sys/types.h>
#elif defined __FreeBSD__ || defined __DragonFly__
#include <fcntl.h>

static int io_fd;
#endif

bool initPassthroughIO(void) {
	if(!passthroughIO_enabled) {                // We may be called more than once.
#if defined __FreeBSD__ || defined __DragonFly__
		io_fd = open("/dev/io", O_RDWR);
		if(io_fd == -1) {
			LOG_MSG("Error: Could not open I/O port device (/dev/io). Root privileges required.");
			return false;
		}
		passthroughIO_enabled = true;
#endif
#if (defined __i386__ || defined __x86_64__) && /* x86 or AMD64 (GCC or Clang) */ \
    (defined __OpenBSD__ || defined __NetBSD__ || (defined LINUX && !defined __ANDROID__)) /* OpenBSD, NetBSD or Linux */
#ifdef LINUX                                    // x86 or AMD64
		if(iopl(3) == -1) {
#elif defined __i386__                          // OpenBSD or NetBSD
		if(i386_iopl(3) == -1) {
#else                                           // AMD64
#ifdef __OpenBSD__
		if(amd64_iopl(3) == -1) {
#else // __NetBSD__
		if(x86_64_iopl(3) == -1) {
#endif
#endif
			LOG_MSG("Error: Could not set the I/O privilege level to 3. Root privileges required.");
			return false;
		}
		passthroughIO_enabled = true;
#endif
	}
	return passthroughIO_enabled;
}

bool dropPrivileges(void) {
	gid_t gid = getgid();
	if(setgid(gid) == -1) {
		LOG_MSG("Error: Could not set GID.");
		return false;
	}
	uid_t uid = getuid();
	if(setuid(uid) == -1) {
		LOG_MSG("Error: Could not set UID.");
		return false;
	}
	return true;
}
#endif
