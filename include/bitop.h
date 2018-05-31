
#include <stdint.h>

namespace bitop {

template <typename T=unsigned int> static inline constexpr unsigned int type_bits(void) {
    return (unsigned int)sizeof(T) * 8u;
}

template <typename T=unsigned int> static inline constexpr T allzero(void) {
    return (T)0;
}

template <typename T=unsigned int> static inline constexpr T allones(void) {
    return (T)( ~((T)0) );
}

template <typename T=unsigned int> static inline constexpr unsigned int _bitlength_recursion(const T v,const unsigned int bits) {
    return (v != allzero()) ? _bitlength_recursion(v >> (T)1u,bits + 1u) : bits;
}

template <typename T=unsigned int> static inline constexpr unsigned int bitlength(const T v) {
    return _bitlength_recursion(v,0);
}

template <typename T=unsigned int> static inline constexpr unsigned int _bitseqlength_recursionlsb(const T v,const unsigned int bits) {
    return (v & 1u) ? _bitseqlength_recursionlsb(v >> (T)1u,bits + 1u) : bits;
}

template <typename T=unsigned int> static inline constexpr unsigned int bitseqlengthlsb(const T v) {
    return _bitseqlength_recursionlsb(v,0);
}

template <const unsigned int a,typename T=unsigned int> static inline constexpr T bit2mask(void) {
    static_assert(a < type_bits<T>(), "bit2mask constexpr bit count too large for data type");
    return (T)1U << (T)a;
}

template <typename T=unsigned int> static inline constexpr T bit2mask(const unsigned int a) {
    return (T)1U << (T)a;
}

template <typename T=unsigned int> static inline constexpr T type_msb_mask(void) {
    return bit2mask<T>(type_bits<T>() - 1u);
}

template <const unsigned int a,const unsigned int offset,typename T=unsigned int> static inline constexpr T bitcount2masklsb(void) {
    /* NTS: special case for a == type_bits because shifting the size of a register OR LARGER is undefined.
     *      On Intel x86 processors, with 32-bit integers, x >> 32 == x >> 0 because only the low 5 bits are used 
     *      a % type_bits<T>() is there to keep a < type_bits<T> in case your C++11 compiler likes to trigger
     *      all static_assert<> clauses even if the ternary would not choose that path. */
    static_assert(a <= type_bits<T>(), "bitcount2masklsb constexpr bit count too large for data type");
    static_assert(offset < type_bits<T>(), "bitcount2masklsb constexpr offset count too large for data type");
    static_assert((a+offset) <= type_bits<T>(), "bitcount2masklsb constexpr bit count + offset count too large for data type");
    return ((a < type_bits<T>()) ? (bit2mask<a % type_bits<T>(),T>() - (T)1u) : allones<T>()) << (T)offset;
}

template <const unsigned int a,typename T=unsigned int> static inline constexpr T bitcount2masklsb(void) {
    return bitcount2masklsb<a,0u,T>();
}

template <typename T=unsigned int> static inline constexpr T bitcount2masklsb(const unsigned int a,const unsigned int offset=0) {
    /* NTS: special case for a == type_bits because shifting the size of a register OR LARGER is undefined.
     *      On Intel x86 processors, with 32-bit integers, x >> 32 == x >> 0 because only the low 5 bits are used */
    return ((a < type_bits<T>()) ? (bit2mask<T>(a) - (T)1u) : allones<T>()) << (T)offset;
}

void self_test();

}

