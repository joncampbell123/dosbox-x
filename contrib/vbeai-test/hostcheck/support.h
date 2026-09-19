#ifndef H_SUPPORT_H
#define H_SUPPORT_H
/* The real include/support.h maps strcasecmp onto stricmp for MSVC; the
 * harness only needs the name to resolve. */
#include <string.h>
#include <strings.h>
#endif
