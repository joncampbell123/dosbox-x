/* Public Domain Curses */

#include "pdcdos.h"

void PDC_beep(void)
{
    PDCREGS regs;

    PDC_LOG(("PDC_beep() - called\n"));

    regs.W.ax = 0x0e07;       /* Write ^G in TTY fashion */
    regs.W.bx = 0;
    PDCINT(0x10, regs);
}

void PDC_napms(int ms)
{
    PDCREGS regs;
    long goal, start, current;

    PDC_LOG(("PDC_napms() - called: ms=%d\n", ms));

    goal = DIVROUND((long)ms, 50);
    if (!goal)
        goal++;

    start = getdosmemdword(0x46c);

    goal += start;

    while (goal > (current = getdosmemdword(0x46c)))
    {
        if (current < start)    /* in case of midnight reset */
            return;

        regs.W.ax = 0x1680;
        PDCINT(0x2f, regs);
        PDCINT(0x28, regs);
    }
}

const char *PDC_sysname(void)
{
    return "DOS";
}

#ifdef __DJGPP__

unsigned char getdosmembyte(int offset)
{
    unsigned char b;

    dosmemget(offset, sizeof(unsigned char), &b);
    return b;
}

unsigned short getdosmemword(int offset)
{
    unsigned short w;

    dosmemget(offset, sizeof(unsigned short), &w);
    return w;
}

unsigned long getdosmemdword(int offset)
{
    unsigned long dw;

    dosmemget(offset, sizeof(unsigned long), &dw);
    return dw;
}

void setdosmembyte(int offset, unsigned char b)
{
    dosmemput(&b, sizeof(unsigned char), offset);
}

void setdosmemword(int offset, unsigned short w)
{
    dosmemput(&w, sizeof(unsigned short), offset);
}

#endif

#if defined(__WATCOMC__) && defined(__386__)

void PDC_dpmi_int(int vector, pdc_dpmi_regs *rmregs)
{
    union REGPACK regs = {0};

    rmregs->w.ss = 0;
    rmregs->w.sp = 0;
    rmregs->w.flags = 0;

    regs.w.ax = 0x300;
    regs.h.bl = vector;
    regs.x.edi = FP_OFF(rmregs);
    regs.x.es = FP_SEG(rmregs);

    intr(0x31, &regs);
}

#endif
