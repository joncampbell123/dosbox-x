#ifndef H_LOGGING_H
#define H_LOGGING_H
#include <stdio.h>
#include <stdarg.h>
enum LOG_TYPES { LOG_MISC };
enum LOG_SEVERITIES { LOG_DEBUG, LOG_WARN, LOG_ERROR, LOG_NORMAL };
extern bool log_verbose;
struct LOG { LOG(LOG_TYPES,LOG_SEVERITIES){}
  void operator()(const char*f,...){ if(!log_verbose) return; va_list a; va_start(a,f);
    fputs("    [log] ",stdout); vprintf(f,a); putchar('\n'); va_end(a); } };
#endif
