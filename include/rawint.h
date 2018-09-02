
#ifndef __ISP_UTILS_MISC_RAWINT_H
#define __ISP_UTILS_MISC_RAWINT_H

#include <stdint.h>

static inline uint16_t __le_u16(const void *p) {
	return *((uint16_t*)(p));
}

static inline void __w_le_u16(const void *p,const uint16_t val) {
	*((uint16_t*)(p)) = val;
}

static inline uint32_t __le_u32(const void *p) {
	return *((uint32_t*)(p));
}

static inline void __w_le_u32(const void *p,const uint32_t val) {
	*((uint32_t*)(p)) = val;
}

static inline uint64_t __le_u64(const void *p) {
	return *((uint64_t*)(p));
}

static inline void __w_le_u64(const void *p,const uint64_t val) {
	*((uint64_t*)(p)) = val;
}

#endif /* __ISP_UTILS_MISC_RAWINT_H */

