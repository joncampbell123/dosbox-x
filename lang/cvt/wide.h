#ifndef __WIDE_H
#define __WIDE_H

#include <cstdint>

/* When this value is set to 1, border character width is considered as 2 */
/* When set to other values, border character width is considered as 1 */
#define BOXDRAWSTYLE 1

#define SEC0_LOW  ((std::uint32_t)0x2500)   /* Define start of CJK wide character section */
#define SEC0_HIGH ((std::uint32_t)0x257F)  
#define SEC1_LOW  ((std::uint32_t)0x2E80)  
#define SEC1_HIGH ((std::uint32_t)0x9FFF)  
#define SEC2_LOW  ((std::uint32_t)0xAC00)  
#define SEC2_HIGH ((std::uint32_t)0xD7AF)  
#define SEC3_LOW  ((std::uint32_t)0xF900)  
#define SEC3_HIGH ((std::uint32_t)0xFA6D)  
#define SEC4_LOW  ((std::uint32_t)0xFF01)
#define SEC4_HIGH ((std::uint32_t)0xFF5E)
#define SEC5_LOW  ((std::uint32_t)0xFFE0)
#define SEC5_HIGH ((std::uint32_t)0xFFE6)
#define SEC6_LOW  ((std::uint32_t)0x1D300)
#define SEC6_HIGH ((std::uint32_t)0x1D35F)
#define SEC7_LOW  ((std::uint32_t)0x20000)
#define SEC7_HIGH ((std::uint32_t)0x2EBEF)
#define SEC8_LOW  ((std::uint32_t)0x2F800)
#define SEC8_HIGH ((std::uint32_t)0x2FA1F)  /* Define end of CJK wide character section */

#endif