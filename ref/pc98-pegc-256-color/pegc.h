#pragma once

#ifdef SUPPORT_PEGC

#define PEGC_REG_PPM_BANK_A8	0x000 // mio1 E0004h
#define PEGC_REG_PPM_BANK_B0	0x002 // mio1 E0006h
#define PEGC_REG_MODE			0x000 // mio2 E0100h
#define PEGC_REG_VRAM_ENABLE	0x002 // mio2 E0102h
#define PEGC_REG_PLANE_ACCESS	0x004 // mio2 E0104h
 // mio2 E0106h
#define PEGC_REG_PLANE_ROP		0x008 // mio2 E0108h
#define PEGC_REG_DATASELECT		0x00A // mio2 E010Ah
#define PEGC_REG_MASK			0x00C // mio2 E010Ch
 // mio2 E010Eh
#define PEGC_REG_LENGTH			0x010 // mio2 E0110h
#define PEGC_REG_SHIFT			0x012 // mio2 E0112h
#define PEGC_REG_PALETTE1		0x014 // mio2 E0114h
 // mio2 E0116h
#define PEGC_REG_PALETTE2		0x018 // mio2 E0118h
#define PEGC_REG_PATTERN		0x020 // mio2 E0120h

typedef struct {
	UINT8 enable; // PEGCプレーンモード使用可能
	
	UINT8 lastdata[64]; // PEGCプレーンモード 最後にVRAMから読み取ったデータ
	SINT32 lastdatalen; // PEGCプレーンモード 読み取り済みデータ長さ
	UINT32 remain; // PEGCプレーンモード 転送データ残り？
} _PEGC, *PEGC;


#ifdef __cplusplus
extern "C" {
#endif
	
REG16 MEMCALL pegc_memvgaplane_rd16(UINT32 address);
void MEMCALL pegc_memvgaplane_wr16(UINT32 address, REG16 value);
UINT32 MEMCALL pegc_memvgaplane_rd32(UINT32 address);
void MEMCALL pegc_memvgaplane_wr32(UINT32 address, UINT32 value);

void pegc_reset(const NP2CFG *pConfig);
void pegc_bind(void);

#ifdef __cplusplus
}
#endif


#endif