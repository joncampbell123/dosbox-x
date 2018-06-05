
#include <stdint.h>

namespace ptrop {

/* return the misalignment in bytes of 'p' in the context of a data type of size 'A',
 * 'A' must be a power of 2, or else this code will not work. */
static inline constexpr uintptr_t misalignment(const uintptr_t p,const uintptr_t A) {
    return p & (A - (uintptr_t)1u);
}

template <const uintptr_t A> static inline constexpr uintptr_t misalignment(const uintptr_t p) { // DEFER
    return misalignment(p,A);
}

template <typename A> static inline constexpr uintptr_t misalignment(const uintptr_t p) { // DEFER
    return misalignment<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr uintptr_t misalignment(T* const p) { // DEFER
    return misalignment<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr uintptr_t misalignment(T* const p) { // DEFER
    return misalignment<A>((uintptr_t)((unsigned char*)p));
}


/* indicate whether pointer 'p' is aligned to type of size 'A' */
static inline constexpr bool isaligned(const uintptr_t p,const uintptr_t A) {
    return misalignment(p,A) == (uintptr_t)0;
}

template <const uintptr_t A> static inline constexpr bool isaligned(const uintptr_t p) { // DEFER
    return isaligned(p,A);
}

template <typename A> static inline constexpr bool isaligned(const uintptr_t p) { // DEFER
    return isaligned<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr bool isaligned(T* const p) { // DEFER
    return isaligned<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr bool isaligned(T* const p) { // DEFER
    return isaligned<A>((uintptr_t)((unsigned char*)p));
}


/* take pointer 'p' and align downward to type of size 'A' */
static inline constexpr uintptr_t aligndown(const uintptr_t p,const uintptr_t A) {
    return p - misalignment(p,A);
}

template <const uintptr_t A> static inline constexpr uintptr_t aligndown(const uintptr_t p) { // DEFER
    return aligndown(p,A);
}

template <typename A> static inline constexpr uintptr_t aligndown(const uintptr_t p) { // DEFER
    return aligndown<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr T* aligndown(T* const p) { // DEFER
    return (T*)aligndown<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr T* aligndown(T* const p) { // DEFER
    return (T*)aligndown<A>((uintptr_t)((unsigned char*)p));
}


/* take pointer 'p' and align upward to type of size 'A' */
static inline constexpr uintptr_t alignup(const uintptr_t p,const uintptr_t A) {
    return aligndown(p + (uintptr_t)A - (uintptr_t)1u,A);
}

template <const uintptr_t A> static inline constexpr uintptr_t alignup(const uintptr_t p) { // DEFER
    return alignup(p,A);
}

template <typename A> static inline constexpr uintptr_t alignup(const uintptr_t p) { // DEFER
    return alignup<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr T* alignup(T* const p) { // DEFER
    return (T*)alignup<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr T* alignup(T* const p) { // DEFER
    return (T*)alignup<A>((uintptr_t)((unsigned char*)p));
}

void self_test(void);

}

