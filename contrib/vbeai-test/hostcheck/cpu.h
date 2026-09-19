#ifndef H_CPU_H
#define H_CPU_H
#include "dosbox.h"
#include "regs.h"
#include "mem.h"
struct CPUBlock { struct { uint32_t mask,notmask; } stack; };
extern CPUBlock cpu;
static inline void CPU_Push16(uint16_t v){
    uint32_t ne=(reg_esp&cpu.stack.notmask)|((reg_esp-2)&cpu.stack.mask);
    mem_writew(SegPhys(ss)+(ne&cpu.stack.mask),v);
    reg_esp=ne;
}
#endif
