
#include <stdio.h> /* NULL */
#include <assert.h>

#include "ptrop.h"

namespace ptrop {

void self_test(void) {
    // DEBUG
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0)      )) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  1u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  2u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  3u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  4u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  5u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  6u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  7u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  8u)) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) +  9u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uint64_t*)((unsigned char*)((void*)0) + 10u)) == false, "whoops" );

    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)0 ) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)1 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)2 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)3 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)4 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)6 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)7 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)8 ) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)9 ) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t>((uintptr_t)10) == false, "whoops" );

    static_assert( ptrop::isaligned((uintptr_t)0, (uintptr_t)8) == true,  "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)1, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)2, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)3, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)4, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)6, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)7, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)8, (uintptr_t)8) == true,  "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)9, (uintptr_t)8) == false, "whoops" );
    static_assert( ptrop::isaligned((uintptr_t)10,(uintptr_t)8) == false, "whoops" );


    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL      )) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  1u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  2u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  3u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  4u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  5u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  6u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  7u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  8u)) == true,  "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL +  9u)) == false, "whoops" );
    static_assert( ptrop::isaligned<uint64_t,8>((uint64_t*)((unsigned char*)NULL + 10u)) == false, "whoops" );

    static_assert( ptrop::isaligned<8>((uintptr_t)0 ) == true,  "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)1 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)2 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)3 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)4 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)6 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)7 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)8 ) == true,  "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)9 ) == false, "whoops" );
    static_assert( ptrop::isaligned<8>((uintptr_t)10) == false, "whoops" );


    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)1 ) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)2 ) == (uintptr_t)2,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)3 ) == (uintptr_t)3,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)4 ) == (uintptr_t)4,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)5 ) == (uintptr_t)5,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)6 ) == (uintptr_t)6,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)7 ) == (uintptr_t)7,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)8 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)9 ) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment<uint64_t>((uintptr_t)10) == (uintptr_t)2,  "whoops" );

    static_assert( ptrop::misalignment<8>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)1 ) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)2 ) == (uintptr_t)2,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)3 ) == (uintptr_t)3,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)4 ) == (uintptr_t)4,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)5 ) == (uintptr_t)5,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)6 ) == (uintptr_t)6,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)7 ) == (uintptr_t)7,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)8 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)9 ) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment<8>((uintptr_t)10) == (uintptr_t)2,  "whoops" );

    static_assert( ptrop::misalignment((uintptr_t)0, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)1, (uintptr_t)8) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)2, (uintptr_t)8) == (uintptr_t)2,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)3, (uintptr_t)8) == (uintptr_t)3,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)4, (uintptr_t)8) == (uintptr_t)4,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)5, (uintptr_t)8) == (uintptr_t)5,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)6, (uintptr_t)8) == (uintptr_t)6,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)7, (uintptr_t)8) == (uintptr_t)7,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)8, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)9, (uintptr_t)8) == (uintptr_t)1,  "whoops" );
    static_assert( ptrop::misalignment((uintptr_t)10,(uintptr_t)8) == (uintptr_t)2,  "whoops" );


    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)1 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)2 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)3 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)4 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)5 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)6 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)7 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)8 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)9 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown<uint64_t>((uintptr_t)10) == (uintptr_t)8,  "whoops" );

    static_assert( ptrop::aligndown<8>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)1 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)2 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)3 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)4 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)5 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)6 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)7 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)8 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)9 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown<8>((uintptr_t)10) == (uintptr_t)8,  "whoops" );

    static_assert( ptrop::aligndown((uintptr_t)0, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)1, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)2, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)3, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)4, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)5, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)6, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)7, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)8, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)9, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::aligndown((uintptr_t)10,(uintptr_t)8) == (uintptr_t)8,  "whoops" );


    static_assert( ptrop::alignup<uint64_t>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)1 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)2 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)3 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)4 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)5 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)6 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)7 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)8 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)9 ) == (uintptr_t)16, "whoops" );
    static_assert( ptrop::alignup<uint64_t>((uintptr_t)10) == (uintptr_t)16, "whoops" );

    static_assert( ptrop::alignup<8>((uintptr_t)0 ) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)1 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)2 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)3 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)4 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)5 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)6 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)7 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)8 ) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)9 ) == (uintptr_t)16, "whoops" );
    static_assert( ptrop::alignup<8>((uintptr_t)10) == (uintptr_t)16, "whoops" );

    static_assert( ptrop::alignup((uintptr_t)0, (uintptr_t)8) == (uintptr_t)0,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)1, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)2, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)3, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)4, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)5, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)6, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)7, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)8, (uintptr_t)8) == (uintptr_t)8,  "whoops" );
    static_assert( ptrop::alignup((uintptr_t)9, (uintptr_t)8) == (uintptr_t)16, "whoops" );
    static_assert( ptrop::alignup((uintptr_t)10,(uintptr_t)8) == (uintptr_t)16, "whoops" );
}

}

