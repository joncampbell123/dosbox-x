
#include "dos_inc.h"
#include "setup.h"
#include "8255.h"

Intel8255::Intel8255() {
    ppiName = NULL;
    pINTR_A = pINTR_B = 0;
    for (unsigned int c=0;c < 3;c++) {
        portNames[c] = NULL;
        for (unsigned int i=0;i < 8;i++)
            pinNames[c][i] = NULL;
    }
}

Intel8255::~Intel8255() {
}

void Intel8255::reset(void) {
    IBF_A  = IBF_B =  0;
    OBF_A  = OBF_B =  0;
    INTE_A = INTE_B = 0;
    INTE_1 = INTE_2 = 0;
    INTR_A = INTR_B = 0;
    writeControl(0x9B); /* default: port A input, port B input, port C input mode 0 mode 0 */
    latchOutPortA = 0;
    latchOutPortB = 0;
    latchOutPortC = 0;
}

uint8_t Intel8255::readPortA(void) {
    IBF_A = true;

    latchOutPortA =
        (latchOutPortA   &   portAWriteMask ) +
        (      inPortA() & (~portAWriteMask));

    updateINTR_A();
    IBF_A = false;
    checkINTR_A();
    return latchOutPortA;
}

uint8_t Intel8255::readPortB(void) {
    IBF_B = true;

    latchOutPortB =
        (latchOutPortB   &   portBWriteMask ) +
        (      inPortB() & (~portBWriteMask));

    updateINTR_B();
    IBF_B = false;
    checkINTR_B();
    return latchOutPortB;
}

uint8_t Intel8255::readPortC(void) {
    latchOutPortC =
        (latchOutPortC   &   portCWriteMask ) +
        (      inPortC() & (~portCWriteMask));
    return latchOutPortC;
}

uint8_t Intel8255::readControl(void) {
    return mode; /* illegal, but probably reads mode byte */
}

void Intel8255::writePortA(uint8_t data,uint8_t mask) {
    mask &= portAWriteMask;
    latchOutPortA = (latchOutPortA & (~mask)) + (data & mask);
    if (mask) {
        OBF_A = true;
        updateINTR_A();
        outPortA(mask);
        checkINTR_A();
    }
}

void Intel8255::writePortB(uint8_t data,uint8_t mask) {
    mask &= portBWriteMask;
    latchOutPortB = (latchOutPortB & (~mask)) + (data & mask);
    if (mask) {
        OBF_B = true;
        updateINTR_B();
        outPortB(mask);
        checkINTR_B();
    }
}

void Intel8255::writePortC(uint8_t data,uint8_t mask) {
    mask &= portCWriteMask;
    latchOutPortC = (latchOutPortC & (~mask)) + (data & mask);
    if (mask) outPortC(mask);
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

        /* update write masks */
        portAWriteMask  =  (mode & 0x10) ? 0x00 : 0xFF;  /* bit 4 */
        portBWriteMask  =  (mode & 0x02) ? 0x00 : 0xFF;  /* bit 1 */
        portCWriteMask  = ((mode & 0x01) ? 0x00 : 0x0F) +/* bit 0 */
                          ((mode & 0x08) ? 0x00 : 0xF0); /* bit 3 */

        /* modes take additional bits from port C */
        if (mode & 0x04) { /* port B mode 1 */
            /* port C meanings:
             *
             * output:
             * bit[2:2] = Acknowledge, from device (IN)
             * bit[1:1] = Output buffer full aka CPU has written data to this port (OUT)
             * bit[0:0] = Interrupt request B (OUT)
             *
             * input:
             * bit[2:2] = Strobe input (loads data into the latch) (IN)
             * bit[1:1] = Input buffer full (OUT)
             * bit[0:0] = Interrupt request B (OUT) */
            portCWriteMask &= ~0x07;
        }
        if (mode & 0x40) { /* port A mode 2 */
            /* port C meanings:
             *
             * bit[7:7] = Output buffer full aka CPU has written data to this port (OUT)
             * bit[6:6] = Acknowledge, from device. This latches the output. Else output is high impedance (IN)
             * bit[5:5] = Input buffer full (OUT)
             * bit[4:4] = Strobe input (loads data into the latch) (IN)
             * bit[3:3] = Interrupt request A (OUT) */
            portCWriteMask &= ~0xF8;
        }
        else if (mode & 0x20) { /* port A mode 1 */
            /* port C meanings:
             *
             * output:
             * bit[7:7] = Output buffer full aka CPU has written data to this port (OUT)
             * bit[6:6] = Acknowledge, from device (IN)
             * bit[3:3] = Interrupt request A (OUT)
             *
             * input:
             * bit[5:5] = Input buffer full (OUT)
             * bit[4:4] = Strobe input (loads data input the latch) (IN) 
             * bit[3:3] = Interrupt request A (OUT) */
            portCWriteMask &= ~((mode & 0x10) ? 0x38 : 0xC8);
        }

        /* according to PC-98 hardware it seems changing a port to input makes the latch forget it's contents */
        latchOutPortA &= ~portAWriteMask;
        latchOutPortB &= ~portBWriteMask;
        latchOutPortC &= ~portCWriteMask;

        /* update */
        outPortA(portAWriteMask);
        outPortB(portBWriteMask);
        outPortC(portCWriteMask);

        /* HACK: I get the impression from the PC-98 platform and "Metal Force" that writing the mode
         *       byte can cause the chip to re-trigger an interrupt. So... */
        /* FIXME: Or am I wrong here, and the retriggering of the interrupt may simply be that internal
         *        interrupts on the PC-98 are level triggered? */
        INTR_A = INTR_B = false;
        checkINTR_A();
        checkINTR_B();

        /* then reset actual state again */
        updateINTR_A();
        updateINTR_B();
        checkINTR_A();
        checkINTR_B();
    }
    else {
        /* bit[7:7] = 0             bit set/reset
         * bit[6:4] = X             don't care
         * bit[3:1] = bit           bit select
         * bit[0:0] = set/reset     1=set 0=reset */
        /* single bit set/reset port C */
        const uint8_t bit = (data >> 1U) & 7U;

        if (mode & 0x40) { /* Port A mode 2 */
            if (bit == 4) {
                INTE_2 = !!(data & 1);
                updateINTR_A();
                checkINTR_A();
            }
            else if (bit == 6) {
                INTE_1 = !!(data & 1);
                updateINTR_A();
                checkINTR_A();
            }
        }
        else if (mode & 0x20) { /* Port A mode 1 */
            if (bit == ((mode & 0x10) ? /*input*/ 4 : /*output*/6)) {
                INTE_A = !!(data & 1);
                updateINTR_A();
                checkINTR_A();
            }
        }

        if (mode & 0x04) { /* Port B mode 1 */
            if (bit == 2) {
                INTE_B = !!(data & 1);
                updateINTR_B();
                checkINTR_B();
            }
        }

        writePortC(/*data*/(data & 1U) << bit,/*mask*/1U << bit);
    }
}

uint8_t Intel8255::inPortA(void) const {
    return 0x00U; /* override this */
}

uint8_t Intel8255::inPortB(void) const {
    return 0x00U; /* override this */
}

uint8_t Intel8255::inPortC(void) const {
    return 0x00U; /* override this */
}

void Intel8255::outPortA(const uint8_t mask) {
    /* override this */
}

void Intel8255::outPortB(const uint8_t mask) {
    /* override this */
}

void Intel8255::outPortC(const uint8_t mask) {
    /* override this */
}

void Intel8255::updateINTR_A(void) {
    if (mode & 0x40) { /* mode 2 */
        INTR_A = (INTE_1 && (!OBF_A)) ^ (INTE_2 && IBF_A); /* OBF goes low when CPU writes, goes high when cleared */
    }
    else if (mode & 0x20) { /* mode 1 */
        if (mode & 0x10)    /* input */
            INTR_A = INTE_A && IBF_A;
        else                /* output */
            INTR_A = INTE_A && (!OBF_A); /* OBF goes low when CPU writes, goes high when cleared */
    }
    else {
        INTR_A = false;
    }
}

void Intel8255::updateINTR_B(void) {
    if (mode & 0x04) { /* mode 1 */
        if (mode & 0x02)    /* input */
            INTR_B = INTE_B && IBF_B;
        else                /* output */
            INTR_B = INTE_B && (!OBF_B); /* OBF goes low when CPU writes, goes high when cleared */
    }
    else {
        INTR_B = false;
    }
}

void Intel8255::checkINTR_A(void) {
    if (pINTR_A != INTR_A) {
        pINTR_A  = INTR_A;
        sigINTR_A();
    }
}

void Intel8255::checkINTR_B(void) {
    if (pINTR_B != INTR_B) {
        pINTR_B  = INTR_B;
        sigINTR_B();
    }
}

void Intel8255::sigINTR_A(void) {
    /* override this */
}

void Intel8255::sigINTR_B(void) {
    /* override this */
}

void Intel8255::strobePortA(void) {
    readPortA(); /* override this */
}

void Intel8255::strobePortB(void) {
    readPortB(); /* override this */
}

void Intel8255::ackPortA(void) {
    OBF_A = false;
    updateINTR_A();
    checkINTR_A();
}

void Intel8255::ackPortB(void) {
    OBF_B = false;
    updateINTR_B();
    checkINTR_B();
}

uint8_t Intel8255::readByPort(uint8_t p03) {
    switch (p03) {
        case 0: return readPortA();
        case 1: return readPortB();
        case 2: return readPortC();
        case 3: return readControl();
    }

    return 0;
}

void Intel8255::writeByPort(uint8_t p03,uint8_t data) {
    switch (p03) {
        case 0: writePortA(data,0xFF); break;
        case 1: writePortB(data,0xFF); break;
        case 2: writePortC(data,0xFF); break;
        case 3: writeControl(data);    break;
    }
}

