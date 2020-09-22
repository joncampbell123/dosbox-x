/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */


/**

   This header contains a bunch of (mostly) system and machine
   dependent functions:

   - timers
   - current time in milliseconds and microseconds
   - debug logging
   - profiling
   - memory locking
   - checking for floating point exceptions

 */

#ifndef _FLUID_SYS_H
#define _FLUID_SYS_H

//#include <glib.h>
#include "fluidsynth_priv.h"


/**
 * Macro used for safely accessing a message from a GError and using a default
 * message if it is NULL.
 * @param err Pointer to a GError to access the message field of.
 * @return Message string
 */
#define fluid_gerror_message(err)  ((err) ? err->message : "No error details")


void fluid_sys_config(void);
void fluid_log_config(void);
void fluid_time_config(void);


/* Misc */

#define FLUID_STMT_START do
#define FLUID_STMT_END while (0)

#define fluid_strify2(x) #x
#define fluid_strify(x) fluid_strify2(x)

#define fluid_return_val_if_fail(expr, ret) \
    FLUID_STMT_START { \
        if (!(expr)) { \
            FLUID_LOG(FLUID_ERR, "condition failed: " #expr); \
            return ret; \
        } \
    } FLUID_STMT_END
#define fluid_return_if_fail(expr)      fluid_return_val_if_fail(expr,)
#define FLUID_INLINE                    inline
#define FLUID_POINTER_TO_UINT(expr)     ((unsigned int) (uintptr_t) (expr))
#define FLUID_UINT_TO_POINTER(expr)     ((void*) (uintptr_t) (expr))
#define FLUID_POINTER_TO_INT(expr)      ((int) (intptr_t) (expr))
#define FLUID_INT_TO_POINTER(expr)      ((void*) (intptr_t) (expr))
#define FLUID_N_ELEMENTS(struct)        (sizeof (struct) / sizeof (struct[0]))
#define FLUID_MEMBER_SIZE(struct, member)  ( sizeof (((struct *)0)->member) )

#ifdef WORDS_BIGENDIAN
#define FLUID_IS_BIG_ENDIAN WORDS_BIGENDIAN
#else
#define FLUID_IS_BIG_ENDIAN FALSE
#endif

/*
 * Utility functions
 */
char *fluid_strtok (char **str, char *delim);


/**

  Additional debugging system, separate from the log system. This
  allows to print selected debug messages of a specific subsystem.
 */

extern unsigned int fluid_debug_flags;

#if DEBUG

enum fluid_debug_level {
  FLUID_DBG_DRIVER = 1
};

int fluid_debug(int level, char * fmt, ...);

#else
#define fluid_debug
#endif


#if defined(__OS2__)
#define INCL_DOS
#include <os2.h>

typedef int socklen_t;
#endif

unsigned int fluid_curtime(void);
double fluid_utime(void);


/**
    Timers

 */

/* if the callback function returns 1 the timer will continue; if it
   returns 0 it will stop */
typedef int (*fluid_timer_callback_t)(void* data, unsigned int msec);

typedef struct _fluid_timer_t fluid_timer_t;

fluid_timer_t* new_fluid_timer(int msec, fluid_timer_callback_t callback,
                               void* data, int new_thread, int auto_destroy,
                               int high_priority);

int delete_fluid_timer(fluid_timer_t* timer);
int fluid_timer_join(fluid_timer_t* timer);
int fluid_timer_stop(fluid_timer_t* timer);

// Macros to use for pre-processor if statements to test which Glib thread API we have (pre or post 2.32)
#define NEW_GLIB_THREAD_API  (GLIB_MAJOR_VERSION > 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION >= 32))
#define OLD_GLIB_THREAD_API  (GLIB_MAJOR_VERSION < 2 || (GLIB_MAJOR_VERSION == 2 && GLIB_MINOR_VERSION < 32))

/* Atomics */
#include "fluid_atomic.h"

/* Muteces */

#include "fluid_mutex.h"

/* Thread private data */

#ifdef HAVE_WINDOWS_H

typedef DWORD fluid_private_t;
#define fluid_private_init(_priv)                  FLUID_STMT_START { \
                                                     if (((_priv) = TlsAlloc()) == TLS_OUT_OF_INDEXES) \
                                                       FLUID_LOG(FLUID_ERR, "Error calling TlsAlloc"); \
                                                   } FLUID_STMT_END
#define fluid_private_free(_priv)                  TlsFree((_priv))
#define fluid_private_get(_priv)                   TlsGetValue((_priv))
#define fluid_private_set(_priv, _data)            FLUID_STMT_START { \
                                                     if (TlsSetValue((_priv), (_data)) == 0) \
                                                       FLUID_LOG(FLUID_ERR, "Error calling pthread_setspecific"); \
                                                   } FLUID_STMT_END

#else

typedef pthread_key_t fluid_private_t;
#define fluid_private_init(_priv)                  FLUID_STMT_START { \
                                                     if (pthread_key_create(&(_priv), NULL) != 0) \
                                                       FLUID_LOG(FLUID_ERR, "Error calling pthread_key_create"); \
                                                   } FLUID_STMT_END
#define fluid_private_free(_priv)
#define fluid_private_get(_priv)                   pthread_getspecific((_priv))
#define fluid_private_set(_priv, _data)            FLUID_STMT_START { \
                                                     if (pthread_setspecific((_priv), (_data)) != 0) \
                                                       FLUID_LOG(FLUID_ERR, "Error calling pthread_setspecific"); \
                                                   } FLUID_STMT_END

#endif

/* Threads */

#ifdef HAVE_WINDOWS_H

typedef HANDLE fluid_thread_t;

#define FLUID_THREAD_ID_NULL            0                       /* A NULL "ID" value */
#define fluid_thread_id_t               DWORD                   /* Data type for a thread ID */
#define fluid_thread_get_id()           GetCurrentThreadId()    /* Get unique "ID" for current thread */

#else

typedef pthread_t fluid_thread_t;

#define FLUID_THREAD_ID_NULL            NULL                    /* A NULL "ID" value */
#define fluid_thread_id_t               fluid_thread_t *        /* Data type for a thread ID */
#define fluid_thread_get_id()           pthread_self()          /* Get unique "ID" for current thread */

#endif

typedef void (*fluid_thread_func_t)(void* data);

fluid_thread_t* new_fluid_thread(const char *name, fluid_thread_func_t func, void *data,
                                 int prio_level, int detach);
void delete_fluid_thread(fluid_thread_t* thread);
void fluid_thread_self_set_prio (int prio_level);
int fluid_thread_join(fluid_thread_t* thread);

/* Sockets and I/O */

fluid_istream_t fluid_get_stdin (void);
fluid_ostream_t fluid_get_stdout (void);
int fluid_istream_readline(fluid_istream_t in, fluid_ostream_t out, char* prompt, char* buf, int len);
int fluid_ostream_printf (fluid_ostream_t out, char* format, ...);

/* The function should return 0 if no error occured, non-zero
   otherwise. If the function return non-zero, the socket will be
   closed by the server. */
typedef int (*fluid_server_func_t)(void* data, fluid_socket_t client_socket, char* addr);

fluid_server_socket_t* new_fluid_server_socket(int port, fluid_server_func_t func, void* data);
int delete_fluid_server_socket(fluid_server_socket_t* sock);
int fluid_server_socket_join(fluid_server_socket_t* sock);
void fluid_socket_close(fluid_socket_t sock);
fluid_istream_t fluid_socket_get_istream(fluid_socket_t sock);
fluid_ostream_t fluid_socket_get_ostream(fluid_socket_t sock);



/* Profiling */


/**
 * Profile numbers. List all the pieces of code you want to profile
 * here. Be sure to add an entry in the fluid_profile_data table in
 * fluid_sys.c
 */
enum {
  FLUID_PROF_WRITE,
  FLUID_PROF_ONE_BLOCK,
  FLUID_PROF_ONE_BLOCK_CLEAR,
  FLUID_PROF_ONE_BLOCK_VOICE,
  FLUID_PROF_ONE_BLOCK_VOICES,
  FLUID_PROF_ONE_BLOCK_REVERB,
  FLUID_PROF_ONE_BLOCK_CHORUS,
  FLUID_PROF_VOICE_NOTE,
  FLUID_PROF_VOICE_RELEASE,
  FLUID_PROF_LAST
};


#if WITH_PROFILING

void fluid_profiling_print(void);


/** Profiling data. Keep track of min/avg/max values to execute a
    piece of code. */
typedef struct _fluid_profile_data_t {
  int num;
  char* description;
  double min, max, total;
  unsigned int count;
} fluid_profile_data_t;

extern fluid_profile_data_t fluid_profile_data[];

/** Macro to obtain a time refence used for the profiling */
#define fluid_profile_ref() fluid_utime()

/** Macro to create a variable and assign the current reference time for profiling.
 * So we don't get unused variable warnings when profiling is disabled. */
#define fluid_profile_ref_var(name)     double name = fluid_utime()

/** Macro to calculate the min/avg/max. Needs a time refence and a
    profile number. */
#define fluid_profile(_num,_ref) { \
  double _now = fluid_utime(); \
  double _delta = _now - _ref; \
  fluid_profile_data[_num].min = _delta < fluid_profile_data[_num].min ? _delta : fluid_profile_data[_num].min; \
  fluid_profile_data[_num].max = _delta > fluid_profile_data[_num].max ? _delta : fluid_profile_data[_num].max; \
  fluid_profile_data[_num].total += _delta; \
  fluid_profile_data[_num].count++; \
  _ref = _now; \
}


#else

/* No profiling */
#define fluid_profiling_print()
#define fluid_profile_ref()  0
#define fluid_profile_ref_var(name)
#define fluid_profile(_num,_ref)

#endif



/**

    Memory locking

    Memory locking is used to avoid swapping of the large block of
    sample data.
 */

#if defined(HAVE_SYS_MMAN_H) && !defined(__OS2__)
#define fluid_mlock(_p,_n)      mlock(_p, _n)
#define fluid_munlock(_p,_n)    munlock(_p,_n)
#else
#define fluid_mlock(_p,_n)      0
#define fluid_munlock(_p,_n)
#endif


/**

    Floating point exceptions

    fluid_check_fpe() checks for "unnormalized numbers" and other
    exceptions of the floating point processsor.
*/
#ifdef FPE_CHECK
#define fluid_check_fpe(expl) fluid_check_fpe_i386(expl)
#define fluid_clear_fpe() fluid_clear_fpe_i386()
#else
#define fluid_check_fpe(expl)
#define fluid_clear_fpe()
#endif

unsigned int fluid_check_fpe_i386(char * explanation_in_case_of_fpe);
void fluid_clear_fpe_i386(void);

/* Sleeping */

#if HAVE_WINDOWS_H
#define fluid_sleep(ms) Sleep(ms)
#else
#define fluid_sleep(ms) usleep((ms) * 1000)
#endif
#if defined(WIN32) && !defined(MINGW32)
#define FLUID_STRCASECMP         _stricmp
#else
#define FLUID_STRCASECMP         strcasecmp
#endif
#endif /* _FLUID_SYS_H */
