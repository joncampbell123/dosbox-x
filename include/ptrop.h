
#include <stdint.h>

namespace ptrop {

template <const uintptr_t A> static inline constexpr uintptr_t misalignment(const uintptr_t p) {
    return p % (uintptr_t)A;
}

template <typename A> static inline constexpr uintptr_t misalignment(const uintptr_t p) {
    return misalignment<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr uintptr_t misalignment(T* const p) {
    return misalignment<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr uintptr_t misalignment(T* const p) {
    return misalignment<A>((uintptr_t)((unsigned char*)p));
}


template <const uintptr_t A> static inline constexpr bool isaligned(const uintptr_t p) {
    return misalignment<A>(p) == 0;
}

template <typename A> static inline constexpr bool isaligned(const uintptr_t p) {
    return misalignment<A>(p) == 0;
}

template <typename T=unsigned char,typename A=T> static inline constexpr bool isaligned(T* const p) {
    return misalignment<T,A>(p) == 0;
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr bool isaligned(T* const p) {
    return misalignment<T,A>(p) == 0;
}


template <const uintptr_t A> static inline constexpr uintptr_t aligndown(const uintptr_t p) {
    return p - misalignment<A>(p);
}

template <typename A> static inline constexpr uintptr_t aligndown(const uintptr_t p) {
    return aligndown<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr T* aligndown(T* const p) {
    return (T*)aligndown<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr T* aligndown(T* const p) {
    return (T*)aligndown<A>((uintptr_t)((unsigned char*)p));
}


template <const uintptr_t A> static inline constexpr uintptr_t alignup(const uintptr_t p) {
    return aligndown<A>(p + (uintptr_t)sizeof(A) - (uintptr_t)1u);
}

template <typename A> static inline constexpr uintptr_t alignup(const uintptr_t p) {
    return alignup<(uintptr_t)sizeof(A)>(p);
}

template <typename T=unsigned char,typename A=T> static inline constexpr T* alignup(T* const p) {
    return (T*)alignup<A>((uintptr_t)((unsigned char*)p));
}

template <typename T=unsigned char,const uintptr_t A> static inline constexpr T* alignup(T* const p) {
    return (T*)alignup<A>((uintptr_t)((unsigned char*)p));
}

void self_test(void);

}

