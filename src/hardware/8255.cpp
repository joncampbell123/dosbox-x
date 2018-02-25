
#include "8255.h"

Intel8255::Intel8255() {
    for (unsigned int c=0;c < 3;c++) {
        for (unsigned int i=0;i < 8;i++)
            inPortNames[c][i] = outPortNames[c][i] = NULL;
    }
}

Intel8255::~Intel8255() {
}

void Intel8255::reset(void) {
    mode = 0x9B;    /* default: port A input, port B input, port C input mode 0 mode 0 */
    outPortA = 0;
    outPortB = 0;
    outPortC = 0;
}

uint8_t Intel8255::readPortA(void) const {
    return  (outPortA   &   portAWriteMask() ) +
            ( inPortA() & (~portAWriteMask()));
}

uint8_t Intel8255::readPortB(void) const {
    return  (outPortB   &   portBWriteMask() ) +
            ( inPortB() & (~portBWriteMask()));
}

uint8_t Intel8255::readPortC(void) const {
    return  (outPortC   &   portCWriteMask() ) +
            ( inPortC() & (~portCWriteMask()));
}

uint8_t Intel8255::readControl(void) const {
    return mode; /* illegal, but probably reads mode byte */
}

void Intel8255::writePortA(uint8_t data,uint8_t mask) {
    mask &= portAWriteMask();
    outPortA = (outPortA & (~mask)) + (data & mask);
}

void Intel8255::writePortB(uint8_t data,uint8_t mask) {
    mask &= portBWriteMask();
    outPortB = (outPortB & (~mask)) + (data & mask);
}

void Intel8255::writePortC(uint8_t data,uint8_t mask) {
    mask &= portCWriteMask();
    outPortC = (outPortC & (~mask)) + (data & mask);
}

void Intel8255::writeControl(uint8_t data) {
    if (data & 0x80) {
        /* bit[7:7] = 1             mode set flag
         * bit[6:5] = mode select   00=mode 0  01=mode 1  1x=mode 2
         * bit[4:4] = Port A        1=input  0=output
         * bit[3:3] = Port C upper  1=input  0=output
         * bit[2:2] = mode select   0=mode 0   1=mode 1
         * bit[1:1] = Port B        1=input  0=output
         * bit[0:0] = Port C lower  1=input  0=output */
        /* mode byte */
        mode = data;

        /* according to PC-98 hardware it seems changing a port to input makes the latch forget it's contents */
        if (mode & 0x01) outPortC &= ~0x0FU;
        if (mode & 0x02) outPortB  =  0x00U;
        if (mode & 0x08) outPortC &= ~0xF0U;
        if (mode & 0x10) outPortA  =  0x00U;
    }
    else {
        /* bit[7:7] = 0             bit set/reset
         * bit[6:4] = X             don't care
         * bit[3:1] = bit           bit select
         * bit[0:0] = set/reset     1=set 0=reset */
        /* single bit set/reset port C */
        const uint8_t bit = (data >> 1U) & 7U;

        writePortC(/*data*/(data & 1U) << bit,/*mask*/1U << bit);
    }
}

uint8_t Intel8255::inPortA(void) const {
    return 0xFFU;
}

uint8_t Intel8255::inPortB(void) const {
    return 0xFFU;
}

uint8_t Intel8255::inPortC(void) const {
    return 0xFFU;
}

