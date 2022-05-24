
#undef mem_readb
#define mem_readb(x) THIS_IS_ILLEGAL_HERE

#undef mem_writeb
#define mem_writeb(x,b) THIS_IS_ILLEGAL_HERE

#undef mem_readw
#define mem_readw(x) THIS_IS_ILLEGAL_HERE

#undef mem_writew
#define mem_writew(x,b) THIS_IS_ILLEGAL_HERE

#undef mem_readd
#define mem_readd(x) THIS_IS_ILLEGAL_HERE

#undef mem_writed
#define mem_writed(x,b) THIS_IS_ILLEGAL_HERE

#undef real_readb
#define real_readb(x) THIS_IS_ILLEGAL_HERE

#undef real_writeb
#define real_writeb(x,b) THIS_IS_ILLEGAL_HERE

#undef real_readw
#define real_readw(x) THIS_IS_ILLEGAL_HERE

#undef real_writew
#define real_writew(x,b) THIS_IS_ILLEGAL_HERE

#undef real_readd
#define real_readd(x) THIS_IS_ILLEGAL_HERE

#undef real_writed
#define real_writed(x,b) THIS_IS_ILLEGAL_HERE

/* NTS: phys_readb/phys_writeb are fine, because they
 *      go directly to system memory. However unless
 *      you're emulating a PCI device with bus mastering
 *      enabled, you should not be using those either. */

