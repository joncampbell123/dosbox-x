#include <assert.h>

#include "bitop.h"

namespace bitop {

void self_test(void) {
    // DEBUG
    static_assert(bitcount2masklsb<0u>() == 0u, "whoops");
    static_assert(bitcount2masklsb<1u>() == 1u, "whoops");
    static_assert(bitcount2masklsb<2u>() == 3u, "whoops");
    static_assert(bitcount2masklsb<2u,1u>() == 6u, "whoops");
    static_assert(bitcount2masklsb<2u,uint8_t>() == 3u, "whoops");
    static_assert(bitcount2masklsb<2u,0u,uint8_t>() == 3u, "whoops");
    static_assert(bitcount2masklsb<2u,1u,uint8_t>() == 6u, "whoops");
    static_assert(bitcount2masklsb<type_bits()>() == allones(), "whoops");
    static_assert(allones<uint32_t>() == (uint32_t)0xFFFFFFFFUL, "whoops");
    static_assert(allzero<uint32_t>() == (uint32_t)0, "whoops");
    static_assert(allones<uint64_t>() == (uint64_t)0xFFFFFFFFFFFFFFFFULL, "whoops");
    static_assert(allzero<uint64_t>() == (uint64_t)0, "whoops");
    static_assert((~allones<uint32_t>()) == allzero<uint32_t>(), "whoops");
    static_assert((~allzero<uint32_t>()) == allones<uint32_t>(), "whoops");
    static_assert((~allones<uint64_t>()) == allzero<uint64_t>(), "whoops");
    static_assert((~allzero<uint64_t>()) == allones<uint64_t>(), "whoops");
    assert(type_bits<uint64_t>() == 64u);
    assert(type_bits<uint32_t>() == 32u);
    assert(type_bits<uint16_t>() == 16u);
    assert(type_bits<uint8_t>() == 8u);
    assert(bit2mask(0u) == 1u);
    assert(bit2mask(8u) == 256u);
    assert(bit2mask<8u>() == 256u);
    static_assert(bit2mask<9u>() == 512u, "whoops");
    static_assert(bit2mask<33u,uint64_t>() == (1ull << 33ull), "whoops");
    static_assert(bitlength<0u>() == 0u, "whoops");
    static_assert(bitlength<1u>() == 1u, "whoops");
    static_assert(bitlength<2u>() == 2u, "whoops");
    static_assert(bitlength<3u>() == 2u, "whoops");
    static_assert(bitlength<4u>() == 3u, "whoops");
    static_assert(bitlength<7u>() == 3u, "whoops");
    static_assert(bitlength<8u>() == 4u, "whoops");
    static_assert(bitlength<~0u>() == type_bits(), "whoops");
    static_assert(type_msb_mask<uint8_t>() == 0x80u, "whoops");
    static_assert(type_msb_mask<uint16_t>() == 0x8000u, "whoops");
    static_assert(type_msb_mask<uint32_t>() == 0x80000000ul, "whoops");
    static_assert(type_msb_mask<uint64_t>() == 0x8000000000000000ull, "whoops");

    static_assert(bitseqlengthlsb<0u>() == 0u, "whoops"); // 0
    static_assert(bitseqlengthlsb<1u>() == 1u, "whoops"); // 1
    static_assert(bitseqlengthlsb<2u>() == 0u, "whoops"); // 10
    static_assert(bitseqlengthlsb<3u>() == 2u, "whoops"); // 11
    static_assert(bitseqlengthlsb<4u>() == 0u, "whoops"); // 100
    static_assert(bitseqlengthlsb<5u>() == 1u, "whoops"); // 101
    static_assert(bitseqlengthlsb<6u>() == 0u, "whoops"); // 110
    static_assert(bitseqlengthlsb<7u>() == 3u, "whoops"); // 111
    static_assert(bitseqlengthlsb<8u>() == 0u, "whoops"); // 1000
    static_assert(bitseqlengthlsb<9u>() == 1u, "whoops"); // 1001
    static_assert(bitseqlengthlsb<10u>() == 0u, "whoops"); // 1010
    static_assert(bitseqlengthlsb<11u>() == 2u, "whoops"); // 1011
    static_assert(bitseqlengthlsb<12u>() == 0u, "whoops"); // 1100
    static_assert(bitseqlengthlsb<15u>() == 4u, "whoops"); // 1111
    static_assert(bitseqlengthlsb<23u>() == 3u, "whoops"); // 10111
    static_assert(bitseqlengthlsb<31u>() == 5u, "whoops"); // 11111
    static_assert(bitseqlengthlsb<~0u>() == type_bits(), "whoops");

    assert(bitlength(0u) == 0u);
    assert(bitlength(1u) == 1u);
    assert(bitlength(2u) == 2u);
    assert(bitlength(3u) == 2u);
    assert(bitlength(4u) == 3u);
    assert(bitlength(7u) == 3u);
    assert(bitlength(255u) == 8u);
    assert(bitlength(256u) == 9u);
    assert(bitlength(512u) == 10u);
    assert(bitlength(1024u) == 11u);
    assert(bitlength(32767u) == 15u);
    assert(bitlength(32768u) == 16u);

    assert(bitseqlengthlsb(0u) == 0u);
    assert(bitseqlengthlsb(1u) == 1u);
    assert(bitseqlengthlsb(2u) == 0u);
    assert(bitseqlengthlsb(3u) == 2u);
    assert(bitseqlengthlsb(4u) == 0u);
    assert(bitseqlengthlsb(5u) == 1u);
    assert(bitseqlengthlsb(6u) == 0u);
    assert(bitseqlengthlsb(7u) == 3u);
    assert(bitseqlengthlsb(255u) == 8u);
    assert(bitseqlengthlsb(256u) == 0u);
    assert(bitseqlengthlsb(512u) == 0u);
    assert(bitseqlengthlsb(1024u) == 0u);
    assert(bitseqlengthlsb(32767u) == 15u);
    assert(bitseqlengthlsb(32768u) == 0u);

    static_assert(bitcount2maskmsb<0u>() == 0u, "whoops");
    static_assert(bitcount2maskmsb<1u>() == (1u << (type_bits() - 1u)), "whoops");
    static_assert(bitcount2maskmsb<2u>() == (3u << (type_bits() - 2u)), "whoops");
    static_assert(bitcount2maskmsb<2u,1u>() == (3u << (type_bits() - 3u)), "whoops");
    static_assert(bitcount2maskmsb<2u,uint8_t>() == (3u << 6u), "whoops");
    static_assert(bitcount2maskmsb<2u,0u,uint8_t>() == (3u << 6u), "whoops");
    static_assert(bitcount2maskmsb<2u,1u,uint8_t>() == (3u << 5u), "whoops");
    static_assert(bitcount2maskmsb<type_bits()>() == allones(), "whoops");

    assert(bitcount2masklsb(0u) == 0u);
    assert(bitcount2masklsb(1u) == 1u);
    assert(bitcount2masklsb(2u) == 3u);
    assert(bitcount2masklsb(2u,1u) == 6u);
    assert(bitcount2masklsb<uint8_t>(2u) == 3u);
    assert(bitcount2masklsb<uint8_t>(2u,0u) == 3u);
    assert(bitcount2masklsb<uint8_t>(2u,1u) == 6u);
    assert(bitcount2masklsb(type_bits()) == allones());

    assert(bitcount2maskmsb(0u) == 0u);
    assert(bitcount2maskmsb(1u) == (1u << (type_bits() - 1u)));
    assert(bitcount2maskmsb(2u) == (3u << (type_bits() - 2u)));
    assert(bitcount2maskmsb(2u,1u) == (3u << (type_bits() - 3u)));
    assert(bitcount2maskmsb<uint8_t>(2u) == (3u << 6u));
    assert(bitcount2maskmsb<uint8_t>(2u,0u) == (3u << 6u));
    assert(bitcount2maskmsb<uint8_t>(2u,1u) == (3u << 5u));
    assert(bitcount2maskmsb(type_bits()) == allones());

    static_assert(ispowerof2(1u) == true, "whoops");
    static_assert(ispowerof2(2u) == true, "whoops");
    static_assert(ispowerof2(3u) == false, "whoops");
    static_assert(ispowerof2(4u) == true, "whoops");
    static_assert(ispowerof2(5u) == false, "whoops");
    static_assert(ispowerof2(6u) == false, "whoops");
    static_assert(ispowerof2(7u) == false, "whoops");
    static_assert(ispowerof2(8u) == true, "whoops");
    static_assert(ispowerof2(9u) == false, "whoops");
    static_assert(ispowerof2(10u) == false, "whoops");
    static_assert(ispowerof2(11u) == false, "whoops");
    static_assert(ispowerof2(255u) == false, "whoops");
    static_assert(ispowerof2(256u) == true, "whoops");
    static_assert(ispowerof2(257u) == false, "whoops");
    static_assert(ispowerof2(32767u) == false, "whoops");
    static_assert(ispowerof2(32768u) == true, "whoops");
    static_assert(ispowerof2(32769u) == false, "whoops");

    assert(ispowerof2(1u) == true);
    assert(ispowerof2(2u) == true);
    assert(ispowerof2(3u) == false);
    assert(ispowerof2(4u) == true);
    assert(ispowerof2(5u) == false);
    assert(ispowerof2(6u) == false);
    assert(ispowerof2(7u) == false);
    assert(ispowerof2(8u) == true);
    assert(ispowerof2(9u) == false);
    assert(ispowerof2(10u) == false);
    assert(ispowerof2(11u) == false);
    assert(ispowerof2(255u) == false);
    assert(ispowerof2(256u) == true);
    assert(ispowerof2(257u) == false);
    assert(ispowerof2(32767u) == false);
    assert(ispowerof2(32768u) == true);
    assert(ispowerof2(32769u) == false);

    static_assert(log2<1u << 31>() == 31, "whoops");
    static_assert(log2<1u << 16>() == 16, "whoops");
    static_assert(log2<1u << 8>() == 8, "whoops");
    static_assert(log2<1u << 4>() == 4, "whoops");
    static_assert(log2<1u << 3>() == 3, "whoops");
    static_assert(log2<1u << 2>() == 2, "whoops");
    static_assert(log2<1u << 1>() == 1, "whoops");
    static_assert(log2<1u << 0>() == 0, "whoops");
    static_assert(log2<256u>() == 8, "whoops");
    static_assert(log2<128u>() == 7, "whoops");
    static_assert(log2<16u>() == 4, "whoops");
    static_assert(log2<1u>() == 0, "whoops");
    static_assert(log2<0u>() == ~0u, "whoops");

                                                    /* 3210 bit position */
                                                    /* ---- */
    static_assert(log2<9u>() == 3, "whoops");       /* 1001 */
    static_assert(log2<7u>() == 2, "whoops");       /*  111 */
    static_assert(log2<5u>() == 2, "whoops");       /*  101 */
    static_assert(log2<3u>() == 1, "whoops");       /*   11 */
    static_assert(log2<2u>() == 1, "whoops");       /*   10 */

    if (sizeof(unsigned long long) >= 8) { /* we're assuming unsigned long long is at least 64 bits here */
    static_assert(log2<unsigned long long,1ull << 63ull>() == 63, "whoops");
    static_assert(log2<unsigned long long,1ull << 48ull>() == 48, "whoops");
    }
    static_assert(log2<unsigned long long,1ull << 31ull>() == 31, "whoops");
    static_assert(log2<unsigned long long,1ull << 16ull>() == 16, "whoops");
    static_assert(log2<unsigned long long,1ull << 8ull>() == 8, "whoops");
    static_assert(log2<unsigned long long,1ull << 4ull>() == 4, "whoops");
    static_assert(log2<unsigned long long,1ull << 3ull>() == 3, "whoops");
    static_assert(log2<unsigned long long,1ull << 2ull>() == 2, "whoops");
    static_assert(log2<unsigned long long,1ull << 1ull>() == 1, "whoops");
    static_assert(log2<unsigned long long,1ull << 0ull>() == 0, "whoops");
    static_assert(log2<unsigned long long,256ull>() == 8, "whoops");
    static_assert(log2<unsigned long long,128ull>() == 7, "whoops");
    static_assert(log2<unsigned long long,16ull>() == 4, "whoops");
    static_assert(log2<unsigned long long,1ull>() == 0, "whoops");
    static_assert(log2<unsigned long long,0ull>() == ~0u, "whoops");

    assert(log2(1u << 31) == 31);
    assert(log2(1u << 16) == 16);
    assert(log2(1u << 8) == 8);
    assert(log2(1u << 4) == 4);
    assert(log2(1u << 3) == 3);
    assert(log2(1u << 2) == 2);
    assert(log2(1u << 1) == 1);
    assert(log2(1u << 0) == 0);
    assert(log2(256u) == 8);
    assert(log2(128u) == 7);
    assert(log2(16u) == 4);
    assert(log2(1u) == 0);
    assert(log2(0u) == ~0u);

                                    /* 3210 bit position */
                                    /* ---- */
    assert(log2(9u) == 3);          /* 1001 */
    assert(log2(7u) == 2);          /*  111 */
    assert(log2(5u) == 2);          /*  101 */
    assert(log2(3u) == 1);          /*   11 */
    assert(log2(2u) == 1);          /*   10 */

    if (sizeof(unsigned long long) >= 8) { /* we're assuming unsigned long long is at least 64 bits here */
    assert(log2<unsigned long long>(1ull << 63ull) == 63);
    assert(log2<unsigned long long>(1ull << 48ull) == 48);
    }
    assert(log2<unsigned long long>(1ull << 31ull) == 31);
    assert(log2<unsigned long long>(1ull << 16ull) == 16);
    assert(log2<unsigned long long>(1ull << 8ull) == 8);
    assert(log2<unsigned long long>(1ull << 4ull) == 4);
    assert(log2<unsigned long long>(1ull << 3ull) == 3);
    assert(log2<unsigned long long>(1ull << 2ull) == 2);
    assert(log2<unsigned long long>(1ull << 1ull) == 1);
    assert(log2<unsigned long long>(1ull << 0ull) == 0);
    assert(log2<unsigned long long>(256ull) == 8);
    assert(log2<unsigned long long>(128ull) == 7);
    assert(log2<unsigned long long>(16ull) == 4);
    assert(log2<unsigned long long>(1ull) == 0);
    assert(log2<unsigned long long>(0ull) == ~0u);

    static_assert(invert<unsigned int>(0) == ~0u, "whoops");
    static_assert(invert<unsigned int>(1) == ~1u, "whoops");
    static_assert(invert<unsigned long>(0) == ~0ul, "whoops");
    static_assert(invert<unsigned long>(1) == ~1ul, "whoops");
    static_assert(invert<unsigned long long>(0) == ~0ull, "whoops");
    static_assert(invert<unsigned long long>(1) == ~1ull, "whoops");

    assert(invert<unsigned int>(0) == ~0u);
    assert(invert<unsigned int>(1) == ~1u);
    assert(invert<unsigned long>(0) == ~0ul);
    assert(invert<unsigned long>(1) == ~1ul);
    assert(invert<unsigned long long>(0) == ~0ull);
    assert(invert<unsigned long long>(1) == ~1ull);

    assert(bitseqlengthandpos(0)   == bitseqlengthandpos_ret_t(0,0));
    assert(bitseqlengthandpos(1)   == bitseqlengthandpos_ret_t(0,1));
    assert(bitseqlengthandpos(3)   == bitseqlengthandpos_ret_t(0,2));
    assert(bitseqlengthandpos(7)   == bitseqlengthandpos_ret_t(0,3));
    assert(bitseqlengthandpos(15)  == bitseqlengthandpos_ret_t(0,4));
    assert(bitseqlengthandpos(31)  == bitseqlengthandpos_ret_t(0,5));
    assert(bitseqlengthandpos(63)  == bitseqlengthandpos_ret_t(0,6));
    assert(bitseqlengthandpos(127) == bitseqlengthandpos_ret_t(0,7));
    assert(bitseqlengthandpos(255) == bitseqlengthandpos_ret_t(0,8));

    assert(bitseqlengthandpos(1)   == bitseqlengthandpos_ret_t(0,1));
    assert(bitseqlengthandpos(2)   == bitseqlengthandpos_ret_t(1,1));
    assert(bitseqlengthandpos(4)   == bitseqlengthandpos_ret_t(2,1));
    assert(bitseqlengthandpos(8)   == bitseqlengthandpos_ret_t(3,1));
    assert(bitseqlengthandpos(16)  == bitseqlengthandpos_ret_t(4,1));
    assert(bitseqlengthandpos(32)  == bitseqlengthandpos_ret_t(5,1));
    assert(bitseqlengthandpos(64)  == bitseqlengthandpos_ret_t(6,1));
    assert(bitseqlengthandpos(128) == bitseqlengthandpos_ret_t(7,1));
    assert(bitseqlengthandpos(256) == bitseqlengthandpos_ret_t(8,1));

    assert(bitseqlengthandpos(2)   == bitseqlengthandpos_ret_t(1,1));
    assert(bitseqlengthandpos(6)   == bitseqlengthandpos_ret_t(1,2));
    assert(bitseqlengthandpos(14)  == bitseqlengthandpos_ret_t(1,3));
    assert(bitseqlengthandpos(30)  == bitseqlengthandpos_ret_t(1,4));
    assert(bitseqlengthandpos(62)  == bitseqlengthandpos_ret_t(1,5));
    assert(bitseqlengthandpos(126) == bitseqlengthandpos_ret_t(1,6));
    assert(bitseqlengthandpos(254) == bitseqlengthandpos_ret_t(1,7));
    assert(bitseqlengthandpos(510) == bitseqlengthandpos_ret_t(1,8));

    assert(bitseqlengthandpos(4)   == bitseqlengthandpos_ret_t(2,1));
    assert(bitseqlengthandpos(12)  == bitseqlengthandpos_ret_t(2,2));
    assert(bitseqlengthandpos(28)  == bitseqlengthandpos_ret_t(2,3));
    assert(bitseqlengthandpos(60)  == bitseqlengthandpos_ret_t(2,4));
    assert(bitseqlengthandpos(124) == bitseqlengthandpos_ret_t(2,5));
    assert(bitseqlengthandpos(252) == bitseqlengthandpos_ret_t(2,6));
    assert(bitseqlengthandpos(508) == bitseqlengthandpos_ret_t(2,7));
    assert(bitseqlengthandpos(1020)== bitseqlengthandpos_ret_t(2,8));
}

}

