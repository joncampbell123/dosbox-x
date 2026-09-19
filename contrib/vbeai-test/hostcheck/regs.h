#ifndef H_REGS_H
#define H_REGS_H
#include "dosbox.h"
struct GPR { union { uint16_t w; struct { uint8_t l,h; } b; }; };
struct RegFile { GPR a,b,c,d; uint16_t si,di; uint32_t esp; uint16_t seg[6]; };
extern RegFile R;
#define reg_ax R.a.w
#define reg_al R.a.b.l
#define reg_ah R.a.b.h
#define reg_bx R.b.w
#define reg_bl R.b.b.l
#define reg_bh R.b.b.h
#define reg_cx R.c.w
#define reg_cl R.c.b.l
#define reg_ch R.c.b.h
#define reg_dx R.d.w
#define reg_dl R.d.b.l
#define reg_dh R.d.b.h
#define reg_si R.si
#define reg_di R.di
#define reg_esp R.esp
enum SegNames { es=0,cs,ss,ds,fs,gs };
static inline uint16_t SegValue(SegNames i){ return R.seg[i]; }
static inline PhysPt SegPhys(SegNames i){ return (PhysPt)R.seg[i]<<4; }
#endif
