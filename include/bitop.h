
#include <stdint.h>

namespace bitop {

/* Return the number of bits of the data type.
 *
 * @return Number of bits in data type T
 */
template <typename T=unsigned int> static inline constexpr unsigned int type_bits(void) {
    return (unsigned int)sizeof(T) * 8u;
}

/* Return data type T with all bits 0
 *
 * @return Type T with zero bits
 */
template <typename T=unsigned int> static inline constexpr T allzero(void) {
    return (T)0;
}

/* Return data type T with all bits 1
 *
 * @return Type T with set bits
 */
template <typename T=unsigned int> static inline constexpr T allones(void) {
    return (T)( ~((T)0) );
}


/* private */
template <typename T=unsigned int> static inline constexpr unsigned int _bitlength_recursion(const T v,const unsigned int bits) {
    return (v != allzero()) ? _bitlength_recursion(v >> (T)1u,bits + 1u) : bits;
}

/* Return minimum number of bits required to represent value 'v' in data type 'T'
 *
 * @return Number of bits needed
 */
template <typename T=unsigned int,const T v> static inline constexpr unsigned int bitlength(void) {
    return _bitlength_recursion<T>(v,0);
}

template <const unsigned int v> static inline constexpr unsigned int bitlength(void) {
    return bitlength<unsigned int,v>();
}

template <typename T=unsigned int> static inline unsigned int bitlength(T v) {
    unsigned int c = 0;

    while ((v & 0xFFUL) == 0xFFUL) {
        v >>= (T)8UL;
        c += (T)8;
    }
    while (v != allzero<T>()) {
        v >>= (T)1UL;
        c++;
    }

    return c;
}


/* Return number of sequential 1 bits counting from LSB in value 'v' of type 'T'
 *
 * @return Number of bits
 */
template <typename T=unsigned int> static inline constexpr unsigned int _bitseqlength_recursionlsb(const T v,const unsigned int bits) {
    return (v & 1u) ? _bitseqlength_recursionlsb<T>(v >> (T)1u,bits + 1u) : bits;
}

template <typename T=unsigned int,const T v> static inline constexpr unsigned int bitseqlengthlsb(void) {
    return _bitseqlength_recursionlsb(v,0);
}

template <const unsigned int v> static inline constexpr unsigned int bitseqlengthlsb(void) {
    return bitseqlengthlsb<unsigned int,v>();
}

template <typename T=unsigned int> static inline void _bitseqlengthlsb_1(unsigned int &c,T &v) {
    while ((v & 0xFFUL) == 0xFFUL) {
        v >>= (T)8UL;
        c += (T)8;
    }
    while (v & 1u) {
        v >>= (T)1UL;
        c++;
    }
}

template <typename T=unsigned int> static inline unsigned int bitseqlengthlsb(T v) {
    unsigned int c = 0;
    _bitseqlengthlsb_1(/*&*/c,/*&*/v);
    return c;
}


/* Return binary mask of bit 'a'
 *
 * @return Bitmask
 */
template <const unsigned int a,typename T=unsigned int> static inline constexpr T bit2mask(void) {
    static_assert(a < type_bits<T>(), "bit2mask constexpr bit count too large for data type");
    return (T)1U << (T)a;
}

template <typename T=unsigned int> static inline constexpr T bit2mask(const unsigned int a) {
    return (T)1U << (T)a;
}


/* Return binary mask of MSB in type 'T'
 *
 * @return Bitmask
 */
template <typename T=unsigned int> static inline constexpr T type_msb_mask(void) {
    return bit2mask<T>(type_bits<T>() - 1u);
}


/* Return binary mask of 'a' LSB bits starting at bit offset 'offset' in type 'T'
 *
 * @return Bitmask
 */
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


/* Return binary mask of 'a' MSB bits starting at bit offset 'offset' in type 'T'
 *
 * @return Bitmask
 */
template <const unsigned int a,const unsigned int offset,typename T=unsigned int> static inline constexpr T bitcount2maskmsb(void) {
    /* NTS: special case for a == type_bits because shifting the size of a register OR LARGER is undefined.
     *      On Intel x86 processors, with 32-bit integers, x >> 32 == x >> 0 because only the low 5 bits are used 
     *      a % type_bits<T>() is there to keep a < type_bits<T> in case your C++11 compiler likes to trigger
     *      all static_assert<> clauses even if the ternary would not choose that path. */
    static_assert(a <= type_bits<T>(), "bitcount2maskmsb constexpr bit count too large for data type");
    static_assert(offset < type_bits<T>(), "bitcount2maskmsb constexpr offset count too large for data type");
    static_assert((a+offset) <= type_bits<T>(), "bitcount2maskmsb constexpr bit count + offset count too large for data type");
    return ((a != (T)0) ? ((T)(allones<T>() << (T)(type_bits<T>() - a)) >> (T)offset) : allzero<T>());
}

template <const unsigned int a,typename T=unsigned int> static inline constexpr T bitcount2maskmsb(void) {
    return bitcount2maskmsb<a,0u,T>();
}

template <typename T=unsigned int> static inline constexpr T bitcount2maskmsb(const unsigned int a,const unsigned int offset=0) {
    /* NTS: special case for a == type_bits because shifting the size of a register OR LARGER is undefined.
     *      On Intel x86 processors, with 32-bit integers, x >> 32 == x >> 0 because only the low 5 bits are used */
    return ((a != (T)0) ? ((T)(allones<T>() << (T)(type_bits<T>() - a)) >> (T)offset) : allzero<T>());
}


/* Indicate whether 'a' is a power of 2.
 *
 * This code will NOT work correctly if a == 0, the result is to be considered undefined.
 *
 * @return Boolean true if 'a' is a power of 2 */
template <typename T=unsigned int> static inline constexpr bool ispowerof2(const unsigned int a) {
    return (a & (a-(T)1u)) == 0;
}

void self_test(void);

}

