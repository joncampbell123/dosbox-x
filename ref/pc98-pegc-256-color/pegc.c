#include	"compiler.h"
#include	"cpucore.h"
#include	"pccore.h"
#include	"iocore.h"
#include	"memegc.h"
#include	"vram.h"

// PEGC プレーンモード
// 関連: vram.c, vram.h, memvga.c, memvga.h

// 詳しくもないのに作ったのでかなりいい加減です。
// 改良するのであれば全部捨てて作り直した方が良いかもしれません

#ifdef SUPPORT_PEGC

// 
REG16 MEMCALL pegc_memvgaplane_rd16(UINT32 address){
	
	int i,j;
	UINT16 ret = 0;
	//UINT8 bit;

	UINT32 addr; // 画素単位の読み込み元アドレス

	//UINT8 src, dst, pat1, pat2; // ソースデータ、ディスティネーションデータ、パターンデータ1&2
	UINT8 ropcode = 0; // ラスタオペレーション設定 E0108h bit0〜bit7
	UINT8 ropmethod = 0; // 論理演算の方法を指定（パターンレジスタまたはカラーパレット） E0108h bit11,10
	UINT8 ropupdmode = 0; // 1ならラスタオペレーションを使用 E0108h bit12
	UINT8 planemask = 0; // プレーン書き込み禁止(0=許可, 1=禁止)　E0104h
	UINT32 pixelmask = 0; // ビット（画素）への書き込み禁止(0=禁止, 1=許可) E010Ch
	UINT32 bitlength = 0; // ブロック転送ビット長(転送サイズ-1) E0110h
	UINT32 srcbitshift = 0; // リード時のビットシフト数 E0112h
	UINT8 shiftdir = 1; // シフト方向（0:inc, 1:dec）E0108h bit9
	UINT8 srccpu = 1; // 直前に読み取ったVRAMデータではなくCPUデータを使用するか E0108h bit8

	// PEGCレジスタ読みだし
	ropcode = LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) & 0xff;
	ropmethod = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 10) & 0x3;
	ropupdmode = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 12) & 0x1;
	planemask = vramop.mio2[PEGC_REG_PLANE_ACCESS];
	pixelmask = LOADINTELDWORD(vramop.mio2 + PEGC_REG_MASK);
	bitlength = LOADINTELDWORD(vramop.mio2 + PEGC_REG_LENGTH) & 0x0fff;
	srcbitshift = (LOADINTELWORD(vramop.mio2+PEGC_REG_SHIFT)) & 0x1f;
	shiftdir = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 9) & 0x1;
	srccpu = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 8) & 0x1;
	
	// 画素単位のアドレス計算
	addr = (address - 0xa8000) * 8;
	addr += srcbitshift;
	addr &= 0x80000-1; // 安全のため

	if(!srccpu){
		if(!shiftdir){
			for(i=0;i<16;i++){
				UINT32 addrtmp = (addr + i) & (0x80000-1); // 読み取り位置
				UINT32 pixmaskpos = (1 << ((15-i+8)&0xf)); // 現在の画素に対応するpixelmaskのビット位置
				UINT8 data = vramex[addrtmp];

				// compare data
				if((data ^ vramop.mio2[PEGC_REG_PALETTE1]) & ~planemask){
					ret |= (1<<i);
				}

				// update last data
				pegc.lastdata[i] = data;

				// update pattern reg
				if((LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 13) & 0x1){
					for(j=7;j>=0;j--){
						UINT16 regdata = LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4);
						regdata = (regdata & ~(1 << i)) | (((data >> j) & 0x1) << i);
						STOREINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4, regdata);
					}
				}
			}
		}else{
			for(i=0;i<16;i++){
				UINT32 addrtmp = (addr - i) & (0x80000-1); // 読み取り位置
				UINT32 pixmaskpos = (1 << ((i/8)*8 + (7-(i&0x7)))); // 現在の画素に対応するvalueやpixelmaskのビット位置
				UINT8 data = vramex[addrtmp];

				// compare data
				if((data ^ vramop.mio2[PEGC_REG_PALETTE1]) & ~planemask){
					ret |= (1<<i);
				}

				// update last data
				pegc.lastdata[i] = data;

				// update pattern reg
				if(LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) & 0x2000){
					for(j=7;j>=0;j--){
						UINT16 regdata = LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4);
						regdata = (regdata & ~(1 << i)) | (((data >> j) & 0x1) << i);
						STOREINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4, regdata);
					}
				}
			}
		}
	}
	pegc.lastdatalen += 16;
	return ret;
}
void MEMCALL pegc_memvgaplane_wr16(UINT32 address, REG16 value){
	
	int i,j;
	UINT8 bit;

	UINT32 addr; // 画素単位の書き込み先アドレス

	UINT8 src, dst, pat1, pat2; // ソースデータ、ディスティネーションデータ、パターンデータ1&2
	UINT8 ropcode = 0; // ラスタオペレーション設定 E0108h bit0〜bit7
	UINT8 ropmethod = 0; // 論理演算の方法を指定（パターンレジスタまたはカラーパレット） E0108h bit11,10
	UINT8 ropupdmode = 0; // 1ならラスタオペレーションを使用 E0108h bit12
	UINT8 planemask = 0; // プレーン書き込み禁止(0=許可, 1=禁止)　E0104h
	UINT32 pixelmask = 0; // ビット（画素）への書き込み禁止(0=禁止, 1=許可) E010Ch
	UINT32 bitlength = 0; // ブロック転送ビット長(転送サイズ-1) E0110h
	UINT32 dstbitshift = 0; // ライト時のビットシフト数 E0112h
	UINT8 shiftdir = 1; // シフト方向（0:inc, 1:dec）E0108h bit9
	UINT8 srccpu = 1; // 直前に読み取ったVRAMデータではなくCPUデータを使用するか E0108h bit8

	// PEGCレジスタ読みだし
	ropcode = LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) & 0xff;
	ropmethod = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 10) & 0x3;
	ropupdmode = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 12) & 0x1;
	planemask = vramop.mio2[PEGC_REG_PLANE_ACCESS];
	pixelmask = LOADINTELDWORD(vramop.mio2 + PEGC_REG_MASK);
	bitlength = LOADINTELDWORD(vramop.mio2 + PEGC_REG_LENGTH) & 0x0fff;
	dstbitshift = (LOADINTELWORD(vramop.mio2+PEGC_REG_SHIFT) >> 8) & 0x1f;
	shiftdir = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 9) & 0x1;
	srccpu = (LOADINTELWORD(vramop.mio2+PEGC_REG_PLANE_ROP) >> 8) & 0x1;
	
	// 画素単位のアドレス計算
	addr = (address - 0xa8000) * 8;
	addr += dstbitshift;
	addr &= 0x80000-1; // 安全のため

	// ???
	bit = (addr & 0x40000)?2:1;
	
	//if(!srccpu && pegc.remain!=0 && pegc.lastdatalen < 16){
	//	return; // 書き込み無視
	//}
	
	if(pegc.remain == 0){
		// データ数戻す?
		pegc.remain = (LOADINTELDWORD(vramop.mio2 + PEGC_REG_LENGTH) & 0x0fff) + 1;
	}
	
	if(!shiftdir){
		// とりあえずインクリメンタル
		// メモ （なんかおかしい）
		//            +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
		//    i       |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 | 15 |
		//            +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
		// value    → bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 bitF bitE bitD bitC bitB bitA bit9 bit8  
		// pixelmask→ bit7 bit6 bit5 bit4 bit3 bit2 bit1 bit0 bitF bitE bitD bitC bitB bitA bit9 bit8  planemask  SRC,DST,PAT
		//                                                                                                  ↓          ↓
		// plane0 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit0        bit0
		// plane1 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit1        bit1
		// plane2 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit2        bit2
		// plane3 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit3        bit3
		// plane4 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit4        bit4
		// plane5 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit5        bit5
		// plane6 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit6        bit6
		// plane7 vramex[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9] [10] [11] [12] [13] [14] [15]    bit7        bit7

		for(i=0;i<16;i++){
			UINT32 addrtmp = (addr + i) & (0x80000-1); // 書き込み位置
			UINT32 pixmaskpos = (1 << ((i/8)*8 + (7-(i&0x7)))); // 現在の画素に対応するvalueやpixelmaskのビット位置
			if(pixelmask & pixmaskpos){ // 書き込み禁止チェック
				// SRCの設定
				if(srccpu){
					// CPUデータを使う
					src = (value & pixmaskpos) ? 0xff : 0x00;
				}else{
					// 直前に読み取ったVRAMデータを使う
					src = pegc.lastdata[i];
				}

				// DSTの設定 現在のVRAMデータ取得
				dst = vramex[addrtmp];

				if(ropupdmode){
					// ROP使用
					vramex[addrtmp] = (vramex[addrtmp] & planemask); // 書き換えされる予定のビットを0にしておく
					
					// PATの設定
					if(ropmethod==0){
						// パターンレジスタを使用
						int col = 0;
						for(j=7;j>=0;j--){
							col |= (LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4) >> i) & 0x1;
							col <<= 1;
						}
						pat1 = pat2 = col;
					}else if(ropmethod==1){
						// パレット2を使用
						pat1 = pat2 = vramop.mio2[PEGC_REG_PALETTE2];
					}else if(ropmethod==2){
						// パレット1を使用
						pat1 = pat2 = vramop.mio2[PEGC_REG_PALETTE1];
					}else if(ropmethod==3){
						// パレット1と2を使用
						pat1 = vramop.mio2[PEGC_REG_PALETTE1];
						pat2 = vramop.mio2[PEGC_REG_PALETTE2];
					}
					// ROP実行
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<7)) vramex[addrtmp] |= (src & dst & pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<6)) vramex[addrtmp] |= (src & dst & ~pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<5)) vramex[addrtmp] |= (src & ~dst & pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<4)) vramex[addrtmp] |= (src & ~dst & ~pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<3)) vramex[addrtmp] |= (~src & dst & pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<2)) vramex[addrtmp] |= (~src & dst & ~pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<1)) vramex[addrtmp] |= (~src & ~dst & pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<0)) vramex[addrtmp] |= (~src & ~dst & ~pat2) & (~planemask);
				}else{
					// SRCの0に対してplanemask&DST, 1に対して~planemask|DST
					vramex[addrtmp] = 0;
					for(j=0;j<8;j++){
						if(src & (1<<j)){
							vramex[addrtmp] |= (~planemask | dst) & (1<<j);
						}else{
							vramex[addrtmp] |= (planemask & dst) & (1<<j);
						}
					}
				}
				// ???
				vramupdate[LOW15(addrtmp >> 3)] |= bit;
			}

			pegc.remain--;
			// 転送サイズチェック
			if(pegc.remain == 0){
				goto endloop; // 抜ける
			}
		}
	}else{
		for(i=0;i<16;i++){
			UINT32 addrtmp = (addr - i) & (0x80000-1); // 書き込み位置
			UINT32 pixmaskpos = (1 << ((i/8)*8 + (7-(i&0x7)))); // 現在の画素に対応するvalueやpixelmaskのビット位置
			if(pixelmask & pixmaskpos){ // 書き込み禁止チェック
				// SRCの設定
				if(srccpu){
					// CPUデータを使う
					src = (value & pixmaskpos) ? 0xff : 0x00;
				}else{
					// 直前に読み取ったVRAMデータを使う
					src = pegc.lastdata[i];
				}

				// DSTの設定 現在のVRAMデータ取得
				dst = vramex[addrtmp];

				if(ropupdmode){
					// ROP使用
					vramex[addrtmp] = (vramex[addrtmp] & planemask); // 書き換えされる予定のビットを0にしておく
					
					// PATの設定
					if(ropmethod==0){
						// パターンレジスタを使用
						int col = 0;
						for(j=7;j>=0;j--){
							col |= (LOADINTELWORD(vramop.mio2 + PEGC_REG_PATTERN + j*4) >> i) & 0x1;
							col <<= 1;
						}
						pat1 = pat2 = col;
					}else if(ropmethod==1){
						// パレット2を使用
						pat1 = pat2 = vramop.mio2[PEGC_REG_PALETTE2];
					}else if(ropmethod==2){
						// パレット1を使用
						pat1 = pat2 = vramop.mio2[PEGC_REG_PALETTE1];
					}else if(ropmethod==3){
						// パレット1と2を使用
						pat1 = vramop.mio2[PEGC_REG_PALETTE1];
						pat2 = vramop.mio2[PEGC_REG_PALETTE2];
					}
					// ROP実行
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<7)) vramex[addrtmp] |= (src & dst & pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<6)) vramex[addrtmp] |= (src & dst & ~pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<5)) vramex[addrtmp] |= (src & ~dst & pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<4)) vramex[addrtmp] |= (src & ~dst & ~pat1) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<3)) vramex[addrtmp] |= (~src & dst & pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<2)) vramex[addrtmp] |= (~src & dst & ~pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<1)) vramex[addrtmp] |= (~src & ~dst & pat2) & (~planemask);
					if(vramop.mio2[PEGC_REG_PLANE_ROP] & (1<<0)) vramex[addrtmp] |= (~src & ~dst & ~pat2) & (~planemask);
				}else{
					// SRCの0に対してplanemask&DST, 1に対して~planemask|DST
					vramex[addrtmp] = 0;
					for(j=0;j<8;j++){
						if(src & (1<<j)){
							vramex[addrtmp] |= (~planemask | dst) & (1<<j);
						}else{
							vramex[addrtmp] |= (planemask & dst) & (1<<j);
						}
					}
				}
				// ???
				vramupdate[LOW15(addrtmp >> 3)] |= bit;
			}

			pegc.remain--;
			// 転送サイズチェック
			if(pegc.remain == 0){
				goto endloop; // 抜ける
			}
		}
	}
endloop:
	gdcs.grphdisp |= bit;
	pegc.lastdatalen -= 16;
}
UINT32 MEMCALL pegc_memvgaplane_rd32(UINT32 address){
	// TODO: 作る
	return 0;
}
void MEMCALL pegc_memvgaplane_wr32(UINT32 address, UINT32 value){
	// TODO: 作る
}

void pegc_reset(const NP2CFG *pConfig) {

	ZeroMemory(&pegc, sizeof(pegc));
	
	pegc.enable = np2cfg.usepegcplane;

	(void)pConfig;
}

void pegc_bind(void) {

}

#endif
