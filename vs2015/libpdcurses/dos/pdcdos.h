/* Public Domain Curses */

#include <curspriv.h>
#include <string.h>

/*----------------------------------------------------------------------
 *  MEMORY MODEL SUPPORT:
 *
 *  MODELS
 *    TINY    cs,ds,ss all in 1 segment (not enough memory!)
 *    SMALL   cs:1 segment, ds:1 segment
 *    MEDIUM  cs:many segments, ds:1 segment
 *    COMPACT cs:1 segment, ds:many segments
 *    LARGE   cs:many segments, ds:many segments
 *    HUGE    cs:many segments, ds:segments > 64K
 */

#ifdef __TINY__
# define SMALL 1
#endif
#ifdef __SMALL__
# define SMALL 1
#endif
#ifdef __MEDIUM__
# define MEDIUM 1
#endif
#ifdef __COMPACT__
# define COMPACT 1
#endif
#ifdef __LARGE__
# define LARGE 1
#endif
#ifdef __HUGE__
# define HUGE 1
#endif

#include <dos.h>

extern short pdc_curstoreal[16];
extern int pdc_adapter;
extern int pdc_scrnmode;
extern int pdc_font;
extern bool pdc_direct_video;
extern bool pdc_bogus_adapter;
extern unsigned pdc_video_seg;
extern unsigned pdc_video_ofs;

#ifdef __DJGPP__        /* Note: works only in plain DOS... */
# if DJGPP == 2
#  define _FAR_POINTER(s,o) ((((int)(s)) << 4) + ((int)(o)))
# else
#  define _FAR_POINTER(s,o) (0xe0000000 + (((int)(s)) << 4) + ((int)(o)))
# endif
# define _FP_SEGMENT(p)     (unsigned short)((((long)p) >> 4) & 0xffff)
#else
# ifdef __TURBOC__
#  define _FAR_POINTER(s,o) MK_FP(s,o)
# else
#  if defined(__WATCOMC__) && defined(__FLAT__)
#   define _FAR_POINTER(s,o) ((((int)(s)) << 4) + ((int)(o)))
#  else
#   define _FAR_POINTER(s,o) (((long)s << 16) | (long)o)
#  endif
# endif
# define _FP_SEGMENT(p)     (unsigned short)(((long)p) >> 4)
#endif
#define _FP_OFFSET(p)       ((unsigned short)p & 0x000f)

#ifdef __DJGPP__
# include <sys/movedata.h>
unsigned char getdosmembyte(int offs);
unsigned short getdosmemword(int offs);
unsigned long getdosmemdword(int offs);
void setdosmembyte(int offs, unsigned char b);
void setdosmemword(int offs, unsigned short w);
#else
# if SMALL || MEDIUM
#  define PDC_FAR far
# else
#  define PDC_FAR
# endif
# define getdosmembyte(offs) \
    (*((unsigned char PDC_FAR *) _FAR_POINTER(0,offs)))
# define getdosmemword(offs) \
    (*((unsigned short PDC_FAR *) _FAR_POINTER(0,offs)))
# define getdosmemdword(offs) \
    (*((unsigned long PDC_FAR *) _FAR_POINTER(0,offs)))
# define setdosmembyte(offs,x) \
    (*((unsigned char PDC_FAR *) _FAR_POINTER(0,offs)) = (x))
# define setdosmemword(offs,x) \
    (*((unsigned short PDC_FAR *) _FAR_POINTER(0,offs)) = (x))
#endif

#if defined(__WATCOMC__) && defined(__386__)

typedef union
{
    struct
    {
        unsigned long edi, esi, ebp, res, ebx, edx, ecx, eax;
    } d;

    struct
    {
        unsigned short di, di_hi, si, si_hi, bp, bp_hi, res, res_hi,
                       bx, bx_hi, dx, dx_hi, cx, cx_hi, ax, ax_hi,
                       flags, es, ds, fs, gs, ip, cs, sp, ss;
    } w;

    struct
    {
        unsigned char edi[4], esi[4], ebp[4], res[4],
                      bl, bh, ebx_b2, ebx_b3, dl, dh, edx_b2, edx_b3,
                      cl, ch, ecx_b2, ecx_b3, al, ah, eax_b2, eax_b3;
    } h;
} pdc_dpmi_regs;

void PDC_dpmi_int(int, pdc_dpmi_regs *);

#endif

#ifdef __DJGPP__
# include <dpmi.h>
# define PDCREGS __dpmi_regs
# define PDCINT(vector, regs) __dpmi_int(vector, &regs)
#else
# ifdef __WATCOMC__
#  ifdef __386__
#   define PDCREGS pdc_dpmi_regs
#   define PDCINT(vector, regs) PDC_dpmi_int(vector, &regs)
#  else
#   define PDCREGS union REGPACK
#   define PDCINT(vector, regs) intr(vector, &regs)
#  endif
# else
#  define PDCREGS union REGS
#  define PDCINT(vector, regs) int86(vector, &regs, &regs)
# endif
#endif

/* Wide registers in REGS: w or x? */

#ifdef __WATCOMC__
# define W w
#else
# define W x
#endif

/* Monitor (terminal) type information */

enum
{
    _NONE, _MDA, _CGA,
    _EGACOLOR = 0x04, _EGAMONO,
    _VGACOLOR = 0x07, _VGAMONO,
    _MCGACOLOR = 0x0a, _MCGAMONO,
    _MDS_GENIUS = 0x30
};

/* Text-mode font size information */

enum
{
    _FONT8 = 8,
    _FONT14 = 14,
    _FONT15,    /* GENIUS */
    _FONT16
};
