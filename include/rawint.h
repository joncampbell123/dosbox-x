
#ifndef __ISP_UTILS_MISC_RAWINT_H
#define __ISP_UTILS_MISC_RAWINT_H

#include <stdint.h>

static inline uint8_t __le_ts_u8(const uint8_t *p) { /* typesafe */
	return *p;
}

static inline uint8_t __le_u8(const void *p) {
	return *((uint8_t*)(p));
}


static inline uint16_t __le_ts_u16(const uint16_t *p) { /* typesafe */
	return *p;
}

static inline uint16_t __le_u16(const void *p) {
	return *((uint16_t*)(p));
}


static inline int8_t __le_ts_s8(const int8_t *p) { /* typesafe */
	return *p;
}

static inline int8_t __le_s8(const void *p) {
	return *((int8_t*)(p));
}


static inline int16_t __le_ts_s16(const int16_t *p) { /* typesafe */
	return *p;
}

static inline int16_t __le_s16(const void *p) {
	return *((int16_t*)(p));
}

/* TODO: typesafe versions of the functions below */

static inline void __w_be_u24(void *_d,uint32_t data) {
	unsigned char *d = (unsigned char*)(_d);
	*d++ = data >> 16;
	*d++ = data >> 8;
	*d++ = data;
}

static inline void __w_le_u16(const void *p,const uint16_t val) {
	*((uint16_t*)(p)) = val;
}

static inline void __w_be_u16(const void *p,const uint16_t val) {
	((uint8_t*)(p))[0] = val >> 8UL;
	((uint8_t*)(p))[1] = val;
}

static inline uint32_t __le_u24(const void *p) {
	return *((uint32_t*)(p)) & 0xFFFFFF;
}

static inline int32_t __le_s24(const void *p) {
	return ((int32_t)(*((uint32_t*)(p)) << 8)) >> 8; /* NTS: unsigned shift to move low 24 bits over, then signed shift to stretch it down */
}

static inline uint32_t __le_u32(const void *p) {
	return *((uint32_t*)(p));
}

static inline int32_t __le_s32(const void *p) {
	return *((int32_t*)(p));
}

static inline void __w_le_u32(const void *p,const uint32_t val) {
	*((uint32_t*)(p)) = val;
}

static inline void __w_be_u32(const void *p,const uint32_t val) {
	((uint8_t*)(p))[0] = val >> 24UL;
	((uint8_t*)(p))[1] = val >> 16UL;
	((uint8_t*)(p))[2] = val >> 8UL;
	((uint8_t*)(p))[3] = val;
}

static inline uint64_t __le_u64(const void *p) {
	return *((uint64_t*)(p));
}

static inline float __le_float32(void *p) {
	return *((float*)(p));
}

static inline double __le_float64(void *p) {
	return *((double*)(p));
}

static inline void __w_le_u64(const void *p,const uint64_t val) {
	*((uint64_t*)(p)) = val;
}

static inline uint16_t __be_u16(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint16_t)c[0]) <<  8U) |
		(((uint16_t)c[1]) <<  0U);
}

static inline int16_t __be_s16(void *p) {
	return (int16_t)__be_u16(p);
}

static inline uint32_t __be_u24(const void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint32_t)c[0]) << 16U) |
		(((uint32_t)c[1]) <<  8U) |
		(((uint32_t)c[2]) <<  0U);
}

static inline int32_t __be_s24(const void *p) {
	return ((int32_t)(__be_u24(p) << 8)) >> 8; /* NTS: unsigned shift to move low 24 bits over, then signed shift to stretch it down */
}

static inline uint32_t __be_u32(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint32_t)c[0]) << 24U) |
		(((uint32_t)c[1]) << 16U) |
		(((uint32_t)c[2]) <<  8U) |
		(((uint32_t)c[3]) <<  0U);
}

static inline int32_t __be_s32(void *p) {
	return (int32_t)__be_u32(p);
}

static inline uint64_t __be_u64(void *p) {
	const unsigned char *c = (const unsigned char *)p;
	return	(((uint64_t)c[0]) << 56ULL) |
		(((uint64_t)c[1]) << 48ULL) |
		(((uint64_t)c[2]) << 40ULL) |
		(((uint64_t)c[3]) << 32ULL) |
		(((uint64_t)c[4]) << 24ULL) |
		(((uint64_t)c[5]) << 16ULL) |
		(((uint64_t)c[6]) <<  8ULL) |
		(((uint64_t)c[7]) <<  0ULL);
}

static inline void __w_be_u64(const void *p,const uint64_t val) {
	((uint8_t*)(p))[0] = val >> 56ULL;
	((uint8_t*)(p))[1] = val >> 48ULL;
	((uint8_t*)(p))[2] = val >> 40ULL;
	((uint8_t*)(p))[3] = val >> 32ULL;
	((uint8_t*)(p))[4] = val >> 24UL;
	((uint8_t*)(p))[5] = val >> 16UL;
	((uint8_t*)(p))[6] = val >> 8UL;
	((uint8_t*)(p))[7] = val;
}

static inline float __be_float32(void *p) {
	union f {
		uint32_t	i;
		float		f;
	};
	union f fv;

	fv.i = __be_u32(p);
	return fv.f;
}

static inline double __be_float64(void *p) {
	union f {
		uint64_t	i;
		double		f;
	};
	union f fv;

	fv.i = __be_u64(p);
	return fv.f;
}

static inline uint16_t __be_to_he_16(uint16_t val) {
	return (val << 8) | (val >> 8);
}

static inline uint16_t __he_to_be_16(uint16_t val) {
	return (val << 8) | (val >> 8);
}

static inline uint32_t __be_to_he_32(uint32_t val) {
	val = ((val & 0xFF00FF00) >> 8) | ((val & 0x00FF00FF) << 8);
	return (val << 16) | (val >> 16);
}

static inline uint32_t __he_to_be_32(uint32_t val) {
	val = ((val & 0xFF00FF00) >> 8) | ((val & 0x00FF00FF) << 8);
	return (val << 16) | (val >> 16);
}

#endif /* __ISP_UTILS_MISC_RAWINT_H */

