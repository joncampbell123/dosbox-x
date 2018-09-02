
#include "shiftjis.h"

ShiftJISDecoder::ShiftJISDecoder() {
    reset();
}

bool ShiftJISDecoder::leadByteWaitingForSecondByte(void) {
    return fullwidth;
}

void ShiftJISDecoder::reset(void) {
    doublewide = false;
    fullwidth = false;
    b1 = b2 = 0;
}

bool ShiftJISDecoder::take(unsigned char c) {
    /* JIS sequence j1, j2
     * ShiftJIS bytes s1, s2
     *
     * if (33 <= j1 && j1 <= 94)
     *   s1 = ((j1 + 1) / 2) + 112
     * else if (95 <= j1 && j1 <= 126)
     *   s1 = ((j1 + 1) / 2) + 176
     * 
     * if (j1 & 1)    <- is odd
     *   s2 = j2 + 31 + (j2 / 96)
     * else           <- is even
     *   s2 = j2 + 126
     */ 
    if (!fullwidth) {
        doublewide = false;
        if (c >= 0x81 && c <= 0x9F) {
            /* Reverse:
             *
             *    s1 = ((j1 + 1) / 2) + 112
             *    s1 - 112 = (j1 + 1) / 2
             *    (s1 - 112) * 2 = j1 + 1
             *    ((s1 - 112) * 2) - 1 = j1 */
            doublewide = fullwidth = true;
            b1 = (c - 112) * 2;
            return false;
        }
        else if (c >= 0xE0 && c <= 0xEF) {
            /* Reverse:
             *
             *    s1 = ((j1 + 1) / 2) + 176
             *    s1 - 176 = (j1 + 1) / 2
             *    (s1 - 176) * 2 = j1 + 1
             *    ((s1 - 176) * 2) - 1 = j1 */
            doublewide = fullwidth = true;
            b1 = (c - 176) * 2;
            return false;
        }
        else {
            b2 = 0;
            b1 = c;
        }
    }
    else { // fullwidth, 2nd byte
        if (c >= 0x9F) { /* j1 is even */
            b2 = c - 126;
        }
        else if (c >= 0x40 && c != 0x7F) { /* j1 is odd */
            b1--; /* (j1 + 1) / 2 */
            b2 = c - 31;
            if (c >= 0x80) b2--; /* gap at 0x7F */
        }
        else {
            // ???
            b1 = 0x7F;
            b2 = 0x7F;
        }

        // some character combinations are actually single-wide such as the
        // proprietary non-standard box drawing characters on PC-98 systems.
        if ((b1 & 0xFC) == 0x28) /* 0x28-0x2B */
            doublewide = false;

        fullwidth = false;
    }

    return true;
}

