#include	"compiler.h"

// PEGC 256 color mode 

// 詳しくもないのに作ったのでかなりいい加減です。
// 改良するのであれば全部捨てて作り直した方が良いかもしれません

#if defined(SUPPORT_PC9821)

#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"memvga.h"
#include	"vram.h"


// ---- macros

#define	VGARD8(p, a) {													\
	UINT32	addr;														\
	addr = (vramop.mio1[(p) * 2] & 15) << 15;							\
	addr += (a);														\
	addr -= (0xa8000 + ((p) * 0x8000));									\
	return(vramex[addr]);												\
}

#define	VGAWR8(p, a, v) {												\
	UINT32	addr;														\
	UINT8	bit;														\
	addr = (vramop.mio1[(p) * 2] & 15) << 15;							\
	addr += (a);														\
	addr -= (0xa8000 + ((p) * 0x8000));									\
	vramex[addr] = (v);													\
	bit = (addr & 0x40000)?2:1;											\
	vramupdate[LOW15(addr >> 3)] |= bit;								\
	gdcs.grphdisp |= bit;												\
}

#define	VGARD16(p, a) {													\
	UINT32	addr;														\
	addr = (vramop.mio1[(p) * 2] & 15) << 15;							\
	addr += (a);														\
	addr -= (0xa8000 + ((p) * 0x8000));									\
	return(LOADINTELWORD(vramex + addr));								\
}

#define	VGAWR16(p, a, v) {												\
	UINT32	addr;														\
	UINT8	bit;														\
	addr = (vramop.mio1[(p) * 2] & 15) << 15;							\
	addr += (a);														\
	addr -= (0xa8000 + ((p) * 0x8000));									\
	STOREINTELWORD(vramex + addr, (v));									\
	bit = (addr & 0x40000)?2:1;											\
	vramupdate[LOW15((addr + 0) >> 3)] |= bit;							\
	vramupdate[LOW15((addr + 1) >> 3)] |= bit;							\
	gdcs.grphdisp |= bit;												\
}

// ---- flat (PEGC 0F00000h-00F80000h Memory Access ?)

REG8 MEMCALL memvgaf_rd8(UINT32 address) {

	return(vramex[address & 0x7ffff]);
}

void MEMCALL memvgaf_wr8(UINT32 address, REG8 value) {

	UINT8	bit;

	address = address & 0x7ffff;
	vramex[address] = value;
	bit = (address & 0x40000)?2:1;
	vramupdate[LOW15(address >> 3)] |= bit;
	gdcs.grphdisp |= bit;
}

REG16 MEMCALL memvgaf_rd16(UINT32 address) {

	address = address & 0x7ffff;
	return(LOADINTELWORD(vramex + address));
}

void MEMCALL memvgaf_wr16(UINT32 address, REG16 value) {

	UINT8	bit;

	address = address & 0x7ffff;
	STOREINTELWORD(vramex + address, value);
	bit = (address & 0x40000)?2:1;
	vramupdate[LOW15((address + 0) >> 3)] |= bit;
	vramupdate[LOW15((address + 1) >> 3)] |= bit;
	gdcs.grphdisp |= bit;
}

UINT32 MEMCALL memvgaf_rd32(UINT32 address){
	return (UINT32)memvgaf_rd16(address)|(memvgaf_rd16(address+2)<<16);
}
void MEMCALL memvgaf_wr32(UINT32 address, UINT32 value){
	memvgaf_wr16(address, (REG16)value);
	memvgaf_wr16(address+2, (REG16)(value >> 16));
}


// ---- 8086 bank memory (PEGC memvga0:A8000h-AFFFFh, memvga1:B0000h-B7FFFh Bank(Packed-pixel Mode) or Plane Access(Plane Mode))

REG8 MEMCALL memvga0_rd8(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return 0;
	}else
#endif
	{
		// Packed-pixel Mode
		VGARD8(0, address)
	}
}
REG8 MEMCALL memvga1_rd8(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return 0;
	}else
#endif
	{
		// Packed-pixel Mode
		VGARD8(1, address)
	}
}
void MEMCALL memvga0_wr8(UINT32 address, REG8 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		// Nothing to do
	}else
#endif
	{
		// Packed-pixel Mode
		VGAWR8(0, address, value)
	}
}
void MEMCALL memvga1_wr8(UINT32 address, REG8 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		// Nothing to do
	}else
#endif
	{
		// Packed-pixel Mode
		VGAWR8(1, address, value)
	}
}

REG16 MEMCALL memvga0_rd16(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return pegc_memvgaplane_rd16(address);
	}else
#endif
	{
		// Packed-pixel Mode
		VGARD16(0, address)
	}
}
REG16 MEMCALL memvga1_rd16(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return pegc_memvgaplane_rd16(address);
	}else
#endif
	{
		// Packed-pixel Mode
		VGARD16(1, address)
	}
}

void MEMCALL memvga0_wr16(UINT32 address, REG16 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		pegc_memvgaplane_wr16(address, value);
	}else
#endif
	{
		// Packed-pixel Mode
		VGAWR16(0, address, value)
	}
}
void MEMCALL memvga1_wr16(UINT32 address, REG16 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		pegc_memvgaplane_wr16(address, value);
	}else
#endif
	{
		// Packed-pixel Mode
		VGAWR16(1, address, value)
	}
}

UINT32 MEMCALL memvga0_rd32(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return pegc_memvgaplane_rd32(address);
	}else
#endif
	{
		// Packed-pixel Mode
		return (UINT32)memvga0_rd16(address)|(memvga0_rd16(address+2)<<16);
	}
}
UINT32 MEMCALL memvga1_rd32(UINT32 address){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		return pegc_memvgaplane_rd32(address);
	}else
#endif
	{
		// Packed-pixel Mode
		return (UINT32)memvga1_rd16(address)|(memvga1_rd16(address+2)<<16);
	}
}
void MEMCALL memvga0_wr32(UINT32 address, UINT32 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		pegc_memvgaplane_wr32(address, value);
	}else
#endif
	{
		// Packed-pixel Mode
		memvga0_wr16(address, (REG16)value);
		memvga0_wr16(address+2, (REG16)(value >> 16));
	}
}
void MEMCALL memvga1_wr32(UINT32 address, UINT32 value){
#ifdef SUPPORT_PEGC
	if(pegc.enable && (vramop.mio2[PEGC_REG_MODE] & 0x1)){
		// Plane Mode
		pegc_memvgaplane_wr32(address, value);
	}else
#endif
	{
		// Packed-pixel Mode
		memvga1_wr16(address, (REG16)value);
		memvga1_wr16(address+2, (REG16)(value >> 16));
	}
}


// ---- 8086 bank I/O (PEGC E0000h-E7FFFh MMIO)

REG8 MEMCALL memvgaio_rd8(UINT32 address) {

	UINT	pos;
	
	if(address > 0xe0000 + 0x0100){
		UINT	pos;
		REG8 ret;
		pos = address - 0xe0000 - 0x0100;
	
		if(PEGC_REG_PATTERN <= pos){
			ret = 0;
			// vramop.mio2[PEGC_REG_PATTERN + ofs] PATTERN DATA (16bit)
			//         pix15 pix14 ...                                          pix1 pix0
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane0   |<---   ofs = 01h           --->|<---   ofs = 00h           --->|
			//  (bit0)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane1   |<---   ofs = 05h           --->|<---   ofs = 04h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane2   |<---   ofs = 09h           --->|<---   ofs = 08h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane3   |<---   ofs = 0Dh           --->|<---   ofs = 0Ch           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane4   |<---   ofs = 11h           --->|<---   ofs = 10h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane5   |<---   ofs = 15h           --->|<---   ofs = 14h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane6   |<---   ofs = 19h           --->|<---   ofs = 18h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane7   |<---   ofs = 1Dh           --->|<---   ofs = 1Ch           --->|
			//  (bit7)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
				// 1 palette x 16 pixels
				//      bit8     〜     bit0
				// pix0 <-- E0120h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit0)
				// pix1 <-- E0124h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit1)
				// pix2 <-- E0128h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit2)
				// pix3 <-- E012Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit3)
				// pix4 <-- E0130h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit4)
				// pix5 <-- E0134h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit5)
				// pix6 <-- E0138h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit6)
				// pix7 <-- E013Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit7)
				// pix8 <-- E0140h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit8)
				// pix9 <-- E0144h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit9)
				// pix10<-- E0148h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit10)
				// pix11<-- E014Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit11)
				// pix12<-- E0150h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit12)
				// pix13<-- E0154h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit13)
				// pix14<-- E0158h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit14)
				// pix15<-- E015Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit15)
				if((pos & 0x3)==0 && pos < 0x60){
					int i;
					int bit = pos / 4;
					for(i=7;i>=0;i--){
						ret |= (vramop.mio2[PEGC_REG_PATTERN + i*4] >> bit) & 0x1;
						ret <<= 1;
					}
				}
			}else{
				// 16 pixels x 8 planes
				//      pix15     〜     pix0
				// bit0 <-- E0120h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x00]
				// bit1 <-- E0124h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x04]
				// bit2 <-- E0128h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x08]
				// bit3 <-- E012Ch(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x0C]
				// bit4 <-- E0130h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x10]
				// bit5 <-- E0134h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x14]
				// bit6 <-- E0138h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x18]
				// bit7 <-- E013Ch(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C]
				if((pos & 0x3)==0 && pos < 0x40){
					ret = vramop.mio2[pos];
				}
			}
			return ret;
		}
	}

	address -= 0xe0000;
	pos = address - 0x0004;
	if (pos < 4) {
		return(vramop.mio1[pos]);
	}
	pos = address - 0x0100;
	if (pos < 0x20) {
		return(vramop.mio2[pos]);
	}
	return(0x00);
}

void MEMCALL memvgaio_wr8(UINT32 address, REG8 value) {

	UINT	pos;
	
	if(address > 0xe0000 + 0x0100){
		UINT	pos;
		pos = address - 0xe0000 - 0x0100;
	
		if(PEGC_REG_PATTERN <= pos){
			if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
				// 1 palette x 8 pixels
				if((pos & 0x3)==0 && pos < 0x60){
					int i;
					int bit = pos / 4;
					for(i=0;i<7;i++){
						UINT8 tmp = vramop.mio2[PEGC_REG_PATTERN + i*4];
						tmp = (tmp & ~(1 << bit)) | ((value & 1) << bit);
						vramop.mio2[PEGC_REG_PATTERN + i*4] = tmp;
						value >>= 1;
					}
				}
			}else{
				// 8 pixels x 8 planes
				if((pos & 0x3)==0 && pos < 0x40){
					vramop.mio2[pos] = value;
				}
			}
			return;
		}
	}

	////if(address == 0xE0110 || address == 0xE0108){
	//	pegc.remain = 0;
	//	//pegc.lastdatalen = 0;
	//	pegc.lastdatalen = -(SINT32)((LOADINTELWORD(vramop.mio2+PEGC_REG_SHIFT)) & 0x1f);
	////}
	address -= 0xe0000;
	pos = address - 0x0004;
	if (pos < 4) {
		vramop.mio1[pos] = value;
		return;
	}
	pos = address - 0x0100;
	if (pos < 0x20) {
		vramop.mio2[pos] = value;
		return;
	}
}

REG16 MEMCALL memvgaio_rd16(UINT32 address) {

	REG16	ret;

	if(address > 0xe0000 + 0x0100){
		UINT	pos;
		pos = address - 0xe0000 - 0x0100;
	
		if(PEGC_REG_PATTERN <= pos){
			ret = 0;
			// vramop.mio2[PEGC_REG_PATTERN + ofs] PATTERN DATA (16bit)
			//         pix15 pix14 ...                                          pix1 pix0
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane0   |<---   ofs = 01h           --->|<---   ofs = 00h           --->|
			//  (bit0)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane1   |<---   ofs = 05h           --->|<---   ofs = 04h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane2   |<---   ofs = 09h           --->|<---   ofs = 08h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane3   |<---   ofs = 0Dh           --->|<---   ofs = 0Ch           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane4   |<---   ofs = 11h           --->|<---   ofs = 10h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane5   |<---   ofs = 15h           --->|<---   ofs = 14h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane6   |<---   ofs = 19h           --->|<---   ofs = 18h           --->|
			//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			// plane7   |<---   ofs = 1Dh           --->|<---   ofs = 1Ch           --->|
			//  (bit7)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
			if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
				// 1 palette x 16 pixels
				//      bit8     〜     bit0
				// pix0 <-- E0120h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit0)
				// pix1 <-- E0124h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit1)
				// pix2 <-- E0128h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit2)
				// pix3 <-- E012Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit3)
				// pix4 <-- E0130h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit4)
				// pix5 <-- E0134h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit5)
				// pix6 <-- E0138h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit6)
				// pix7 <-- E013Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit7)
				// pix8 <-- E0140h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit8)
				// pix9 <-- E0144h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit9)
				// pix10<-- E0148h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit10)
				// pix11<-- E014Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit11)
				// pix12<-- E0150h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit12)
				// pix13<-- E0154h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit13)
				// pix14<-- E0158h(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit14)
				// pix15<-- E015Ch(8bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit15)
				if((pos & 0x3)==0 && pos < 0x60){
					int i;
					int bit = pos / 4;
					for(i=7;i>=0;i--){
						ret |= (LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4) >> bit) & 0x1;
						ret <<= 1;
					}
				}
			}else{
				// 16 pixels x 8 planes
				//      pix15     〜     pix0
				// bit0 <-- E0120h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x00]
				// bit1 <-- E0124h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x04]
				// bit2 <-- E0128h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x08]
				// bit3 <-- E012Ch(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x0C]
				// bit4 <-- E0130h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x10]
				// bit5 <-- E0134h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x14]
				// bit6 <-- E0138h(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x18]
				// bit7 <-- E013Ch(16bit) -->     LOADINTELWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C]
				if((pos & 0x3)==0 && pos < 0x40){
					ret = LOADINTELWORD(vramop.mio2 + pos);
				}
			}
			return ret;
		}
	}

	ret = memvgaio_rd8(address);
	ret |= memvgaio_rd8(address + 1) << 8;
	return(ret);
}

void MEMCALL memvgaio_wr16(UINT32 address, REG16 value) {
	
	if(address > 0xe0000 + 0x0100){
		UINT	pos;
		pos = address - 0xe0000 - 0x0100;
	
		if(PEGC_REG_PATTERN <= pos){
			if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
				// 1 palette x 16 pixels
				if((pos & 0x3)==0 && pos < 0x60){
					int i;
					int bit = pos / 4;
					for(i=0;i<7;i++){
						UINT16 tmp = LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4);
						tmp = (tmp & ~(1 << bit)) | ((value & 1) << bit);
						STOREINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4, tmp);
						value >>= 1;
					}
				}
			}else{
				// 16 pixels x 8 planes
				if((pos & 0x3)==0 && pos < 0x40){
					STOREINTELWORD(vramop.mio2 + pos, value);
				}
			}
			return;
		}
	}

	memvgaio_wr8(address + 0, (REG8)value);
	memvgaio_wr8(address + 1, (REG8)(value >> 8));
	
}

UINT32 MEMCALL memvgaio_rd32(UINT32 address){
	
	UINT32	ret;
	UINT	pos;

	pos = address - 0xe0000 - 0x0100;
	
	if(address > 0xe0000 + 0x0100 && PEGC_REG_PATTERN <= pos){
		ret = 0;
		// vramop.mio2[PEGC_REG_PATTERN + ofs] PATTERN DATA (32bit)
		//         pix31 pix30 ...                                                                                                          pix1 pix0
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane0   |<---   ofs = 03h           --->|<---   ofs = 02h           --->|<---   ofs = 01h           --->|<---   ofs = 00h           --->|
		//  (bit0)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane1   |<---   ofs = 07h           --->|<---   ofs = 06h           --->|<---   ofs = 05h           --->|<---   ofs = 04h           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane2   |<---   ofs = 0Bh           --->|<---   ofs = 0Ah           --->|<---   ofs = 09h           --->|<---   ofs = 08h           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane3   |<---   ofs = 0Fh           --->|<---   ofs = 0Eh           --->|<---   ofs = 0Dh           --->|<---   ofs = 0Ch           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane4   |<---   ofs = 13h           --->|<---   ofs = 12h           --->|<---   ofs = 11h           --->|<---   ofs = 10h           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane5   |<---   ofs = 17h           --->|<---   ofs = 16h           --->|<---   ofs = 15h           --->|<---   ofs = 14h           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane6   |<---   ofs = 1Bh           --->|<---   ofs = 1Ah           --->|<---   ofs = 19h           --->|<---   ofs = 18h           --->|
		//          +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		// plane7   |<---   ofs = 1Fh           --->|<---   ofs = 1Eh           --->|<---   ofs = 1Dh           --->|<---   ofs = 1Ch           --->|
		//  (bit7)  +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
		if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
			// 1 palette x 32 pixels
			//      bit8     〜     bit0
			// pix0 <-- E0120h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit0)
			// pix1 <-- E0124h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit1)
			// pix2 <-- E0128h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit2)
			// pix3 <-- E012Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit3)
			// pix4 <-- E0130h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit4)
			// pix5 <-- E0134h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit5)
			// pix6 <-- E0138h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit6)
			// pix7 <-- E013Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit7)
			// pix8 <-- E0140h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit8)
			// pix9 <-- E0144h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit9)
			// pix10<-- E0148h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit10)
			// pix11<-- E014Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit11)
			// pix12<-- E0150h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit12)
			// pix13<-- E0154h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit13)
			// pix14<-- E0158h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit14)
			// pix15<-- E015Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit15)
			// pix16<-- E0160h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit16)
			// pix17<-- E0164h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit17)
			// pix18<-- E0168h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit18)
			// pix19<-- E016Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit19)
			// pix20<-- E0170h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit20)
			// pix21<-- E0174h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit21)
			// pix22<-- E0178h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit22)
			// pix23<-- E017Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit23)
			// pix24<-- E0180h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit24)
			// pix25<-- E0184h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit25)
			// pix26<-- E0188h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit26)
			// pix27<-- E018Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit27)
			// pix28<-- E0190h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit28)
			// pix29<-- E0194h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit29)
			// pix30<-- E0198h(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit30)
			// pix31<-- E019Ch(8bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C], vramop.mio2[PEGC_REG_PATTERN + 0x18], ... , vramop.mio2[PEGC_REG_PATTERN + 0x00] (bit31)
			if((pos & 0x3)==0 && pos < 0x100){
				int i;
				int bit = pos / 4;
				for(i=7;i>=0;i--){
					ret |= (LOADINTELDWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4) >> bit) & 0x1;
					ret <<= 1;
				}
			}
		}else{
			// 32 pixels x 8 planes
			//      pix31     〜     pix0
			// bit0 <-- E0120h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x00]
			// bit1 <-- E0124h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x04]
			// bit2 <-- E0128h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x08]
			// bit3 <-- E012Ch(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x0C]
			// bit4 <-- E0130h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x10]
			// bit5 <-- E0134h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x14]
			// bit6 <-- E0138h(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x18]
			// bit7 <-- E013Ch(32bit) -->     LOADINTELDWORD vramop.mio2[PEGC_REG_PATTERN + 0x1C]
			if((pos & 0x3)==0 && pos < 0x40){
				ret = LOADINTELDWORD(vramop.mio2 + pos);
			}
		}
		return ret;
	}

	return (UINT32)memvgaio_rd16(address)|(memvgaio_rd16(address+2)<<16);
}
void MEMCALL memvgaio_wr32(UINT32 address, UINT32 value){
	
	UINT	pos;

	pos = address - 0xe0000 - 0x0100;
	
	if(address > 0xe0000 + 0x0100 && PEGC_REG_PATTERN <= pos){
		if(vramop.mio2[PEGC_REG_PLANE_ROP] & 0x8000){
			// 1 palette x 32 pixels
			if((pos & 0x3)==0 && pos < 0x100){
				int i;
				int bit = pos / 4;
				for(i=0;i<7;i++){
					UINT32 tmp = LOADINTELDWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4);
					tmp = (tmp & ~(1 << bit)) | ((value & 1) << bit);
					STOREINTELDWORD(vramop.mio2 + PEGC_REG_PATTERN + i*4, tmp);
					value >>= 1;
				}
			}
		}else{
			// 32 pixels x 8 planes
			if((pos & 0x3)==0 && pos < 0x40){
				STOREINTELDWORD(vramop.mio2 + pos, value);
			}
		}
		return;
	}

	memvgaio_wr16(address, (REG16)value);
	memvgaio_wr16(address+2, (REG16)(value >> 16));
}

#endif

