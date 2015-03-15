
#ifndef __ISP_UTILS_V4_MISC_INFORMATIONAL_H
#define __ISP_UTILS_V4_MISC_INFORMATIONAL_H

/* informational #defines that may or may not have any effect on the data type,
 * but serve as help for the programmer */

/* Endianness reminders & markers.
 * And someday, when compiling for, say, PowerPC, it will map
 * to whatever native datatype typedefs that tell PowerPC what endiannes to use */
#define _Little_Endian_
#define _Big_Endian_
#define _Mixed_Endian_

/* Microsoft SDK style direction indicators */
#define _In_ /* parameter passed in */
#define _In_opt_ /* parameter passed in, if pointer, can be NULL */
#define _Out_ /* parameter (usually a pointer) through which something is returned */
#define _Out_opt_ /* same as _Out_ but can be NULL (returned value not desired) */
#define _Inout_ /* parameter (pointer) takes in data, and returns data */
#define _Inout_opt_ /* optional form of _Inout_ */

#endif /* __ISP_UTILS_V4_MISC_INFORMATIONAL_H */

