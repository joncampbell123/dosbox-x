#ifndef H_MEM_H
#define H_MEM_H
#include "dosbox.h"
extern uint8_t GuestMem[0x110000];
static inline void phys_writeb(PhysPt a,uint8_t v){ GuestMem[a&0xFFFFF]=v; }
static inline void phys_writew(PhysPt a,uint16_t v){ phys_writeb(a,(uint8_t)v); phys_writeb(a+1,(uint8_t)(v>>8)); }
static inline void phys_writed(PhysPt a,uint32_t v){ phys_writew(a,(uint16_t)v); phys_writew(a+2,(uint16_t)(v>>16)); }
static inline uint8_t phys_readb(PhysPt a){ return GuestMem[a&0xFFFFF]; }
static inline uint16_t phys_readw(PhysPt a){ return (uint16_t)(phys_readb(a)|(phys_readb(a+1)<<8)); }
static inline uint32_t phys_readd(PhysPt a){ return (uint32_t)phys_readw(a)|((uint32_t)phys_readw(a+2)<<16); }
static inline uint8_t mem_readb(PhysPt a){ return phys_readb(a); }
static inline uint16_t mem_readw(PhysPt a){ return phys_readw(a); }
static inline uint32_t mem_readd(PhysPt a){ return phys_readd(a); }
static inline void mem_writew(PhysPt a,uint16_t v){ phys_writew(a,v); }
static inline RealPt RealMake(uint16_t s,uint16_t o){ return ((RealPt)s<<16)|o; }
static inline uint16_t RealSeg(RealPt p){ return (uint16_t)(p>>16); }
static inline uint16_t RealOff(RealPt p){ return (uint16_t)(p&0xffff); }
static inline PhysPt PhysMake(uint16_t s,uint16_t o){ return ((PhysPt)s<<4)+o; }
static inline uint32_t real_readd(uint16_t s,uint16_t o){ return phys_readd(PhysMake(s,o)); }
#endif
