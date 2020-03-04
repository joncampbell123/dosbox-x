/* master.lib 		98雑用ライブラリ
 * Version 0.22j
 *
 *	mgr.h - master.lib を gr.lib用プログラムで使うヘッダファイル
 *
 *	master.lib 0.22d:	(C)1994 恋塚,Kazumi,steelman
 *	gr.lib 1.02:		(C)1990-92 metys, TAKA, Danna
 *
 *		※ gr.lib のすべての機能をカバーするわけではありません。
 *		   また、正しく互換をとっているとも限りません。
 *
 *		※ マクロでカバーするために引数を数回評価しているものが
 *		   あるので、その場合意図しない動作をする場合もあります。
 *
 *		※ あくまでもその場しのぎ的マクロなので、実行効率は
 *		   master.lib を直接使用する方が良くなるでせう。
 *
 *		※ master.lib の全機能が併用できます。まあ注意がいるのも
 *		   あるかもしれないけど。
 */
#ifndef __MGR_H
#define __MGR_H

#ifndef __MASTER_H
# include "master.h"
#endif

#if __MASTER_VERSION < 22
# error master.lib 0.22以上が必要です!!
#endif

/*
 *	type define
 */
#ifndef	_UNSIGNED_DEFINED
typedef unsigned char BYTE ;
typedef unsigned int WORD ;
typedef unsigned long DWORD ;
typedef char * PSTR ;
typedef char far * LPSTR ;
typedef int far * LPINT ;
typedef unsigned char far * LPBYTE ;
#define	_UNSIGNED_DEFINED
#endif

#define _MOUSE_INFO_DEFINED
typedef struct mouse_info MOUSE_INFO ;

#define	_FILE_INFO_DEFINED
#if __TURBOC__ > 0 && __TURBOC__ < 0x300
typedef struct find_t {
    char reserved[21] ;
    char attrib ;
    unsigned wr_time ;
    unsigned wr_date ;
    long size ;
    char name[13] ;
} ;
#endif
typedef struct find_t FILE_INFO ;

typedef	struct{
	WORD	seg,
			no;
} EMS_PHYS_PAGE;

/*
 *	error code
 */
#define		OK			0
#define		ER			-1
#define		CANCEL		0x1b
enum GR_ERROR {
	FILE_ER		= 0x100,
	OPEN_ER,
	READ_ER,
	WRITE_ER,
	FILEEND,
	FILECHG,
	MEMORY_ER	= 0x200,
	DATA_ER		= 0x300
};

/*	(旧バージョン互換用)	*/
#define		NG				1
#define		BFNT_NOTFOUND	-11
#define		BFNT_ILLIGAL	-12

/*
 * colors and attributes of TEXT VRAM
 */
#define	TXT_BLACK		TX_BLACK
#define	TXT_BLUE		TX_BLUE
#define	TXT_RED			TX_RED
#define	TXT_MAGENTA		TX_MAGENTA
#define	TXT_GREEN		TX_GREEN
#define	TXT_CYAN		TX_CYAN
#define	TXT_YELLOW		TX_YELLOW
#define	TXT_WHITE		TX_WHITE

#define	TXT_UNDERLINE	TX_UNDERLINE
#define	TXT_REVERSE		TX_REVERSE
#define	TXT_BLINK		TX_BLINK

/*
 *	"SetKey" set key code
 */
#define	KEY_ESC			K_ESC
#define	KEY_CR			K_CR
#define	KEY_TAB			K_TAB
#define	KEY_ROLLUP		K_ROLLUP
#define	KEY_ROLLDOWN	K_ROLLDOWN
#define	KEY_INS			K_INS
#define	KEY_DEL			K_DEL
#define	KEY_HELP		K_HELP
#define	KEY_HOMECLR		K_HOMECLR
#define	KEY_BS			K_BS
#define	KEY_UP			K_UP
#define	KEY_DOWN		K_DOWN
#define	KEY_LEFT		K_LEFT
#define	KEY_RIGHT		K_RIGHT

#define	KEY_SHIFT		K_SHIFT
#define	KEY_CAPS		K_CAPS
#define	KEY_KANA		K_KANA
#define	KEY_GRPH		K_GRPH
#define	KEY_CTRL		K_CTRL

#define	KEY_S_UP		K_S_UP
#define	KEY_S_DOWN		K_S_DOWN
#define	KEY_S_LEFT		K_S_LEFT
#define	KEY_S_RIGHT		K_S_RIGHT

/*
 *	graphic VRAM segment address
 */
#define		BLUE_P		0xa800		/*	青プレーン	*/
#define		RED_P		0xb000		/*	赤プレーン	*/
#define		GREEN_P		0xb800		/*	緑プレーン	*/
#define		INTENSITY_P	0xe000		/*	輝度プレーン*/

/*
 *	plane value defines for GRCG
 */
#define		GRCG_BLUE		1		/*	青プレーン	*/
#define		GRCG_RED		2		/*	赤プレーン	*/
#define		GRCG_GREEN		4		/*	緑プレーン	*/
#define		GRCG_INTENSITY	8		/*	輝度プレーン*/

#define cline(x1,y1,x2,y2,color)	grcg_setcolor(GC_RMW,color),\
									grcg_line(x1,y1,x2,y2),grcg_off()
#define gbox(x1,y1,x2,y2,color)		grcg_setcolor(GC_RMW,color),\
									grcg_boxfill(x1,y1,x2,y2),grcg_off()

#define timeStart()		vsync_start()
#define	timeStop()		vsync_end()
#define	timeSpent()		vsync_Count1
#define	timeSpent2()	vsync_Count2
#define	timeReset()		vsync_reset1()
#define	timeReset2()	vsync_reset2()
#define COUNT			vsync_Count1
#define COUNT2			vsync_Count2
#define	WaitVsync()		vsync_wait()

#define circlex(x,y,r,color)	grcg_setcolor(GC_RMW,color),\
								grcg_circle_x(x,y,r),grcg_off()

/*
 *	graphic
 */
#define	grVPage(page)		graph_showpage(page)
#define	grAPage(page)		graph_accesspage(page)
#define grStart()			bios30_push(),(bios30_tt_exist()&BIOS30_TT ? \
					bios30_setmode(BIOS30_NORMAL) : \
					(bios30_setline(25),bios30_setmode(BIOS30_NORMALLINE))),\
					graph_start(),memcpy(Palettes[8],Palettes[0],sizeof (Palettes[0])*8),palette_show()
#define grEnd()				graph_end(),bios30_pop()
#define	grOn()				graph_show()
#define	grOff()				graph_hide()
#define grPal(pal)			palette_set_all_16(pal),palette_show()
#define grPalTone(pal,tone)	palette_set_all_16(pal),\
							palette_settone((tone)*100/128),(PaletteTone=100)
#define	grPal1(color,r,g,b)	palette_set(color,(r)*17,(g)*17,(b)*17),\
							palette_show()
#define gr200(lower)		graph_200line(lower)
#define gr400()				graph_400line()
#define grCls()				graph_clear()
#define grBox(x1,y1,x2,y2,color)	gbox(x1,y1,x2,y2,color)
#define grByteBox(x1,y1,x2,y2,color) grcg_setcolor(GC_RMW,color),\
								grcg_byteboxfill_x(x1,y1,x2,y2),grcg_off()
#define grCircle(x,y,r,color)	grcg_setcolor(GC_RMW,color),\
								grcg_circle(x,y,r),grcg_off()
#define grCircleFill(x,y,r,color)	grcg_setcolor(GC_RMW,color),\
								grcg_circlefill(x,y,r),grcg_off()
#define	grCirclexFill(x,y,r,color)	grCircleFill(x,y,r,color)
#define grRectangle(x1,y1,x2,y2,color) grcg_setcolor(GC_RMW,color),\
								grcg_hline(x1,x2,y1),grcg_hline(x1,x2,y2),\
								grcg_vline(x1,y1,y2),grcg_vline(x2,y1,y2),\
								grcg_off()
#define grHLine(x1,x2,y,color)	grcg_setcolor(GC_RMW,color),\
								grcg_hline(x1,x2,y),grcg_off()
#define grVLine(x,y1,y2,color )	grcg_setcolor(GC_RMW,color),\
								grcg_vline(x,y1,y2),grcg_off()
#define grPSet(x,y,color)		grcg_setcolor(GC_RMW,color),\
								grcg_pset(x,y),grcg_off()
#define grPGet(x,y)				graph_readdot(x,y)
#define grBlank(x,y)			(graph_readdot(x,y)==0)
#define grStoreVram(useMain)	!graph_backup(G_PAGE0)
#define grRestoreVram()			graph_restore()
#define grCopyPage(from,to)		graph_copy_page(to)
#define grScrollY(y)			graph_scrollup(y)
#define grScrollX(x)			graph_scroll(graph_VramLines,(x),0)
#define grScrollXY(x,y)			graph_scroll(graph_VramLines-(y),(x)+(y)*40,(x))
#define grSetResPal(makeNew)	(makenew ? respal_create() : respal_exist()),\
								respal_set_palettes()
#define grGetResPal()			respal_get_palettes()
#define grSetID()

#define	gr_psetnext(x,y)	grcg_pset(x,y)
#define	gdcset()

/*
 *	青プレーン描画
 */
#define	bpCls()					grcg_fill()
#define bpPSet(x,y)				grcg_pset(x,y)
#define bpBox(x1,y1,x2,y2)		grcg_boxfill(x1,y1,x2,y2)
#define bpByteBox(x1,y1,x2,y2)	grcg_byteboxfill_x(x1,y1,x2,y2)
#define bpCircle(x,y,r)			grcg_circle(x,y,r)
#define bpCircleFill(x,y,r)		grcg_circlefill(x,y,r)
#define bpCirclexFill(x,y,r)	grcg_circlefill(x,y,r)
#define bpHLine(x1,x2,y)		grcg_hline(x1,x2,y)
#define bpVLine(x,y1,y2)		grcg_vline(x,y1,y2)

/*
 *	GRCG mode set
 */
#define grcgColor(color)			grcg_setcolor(GC_RMW,color)
#define grcgPlane(activeplane)		grcg_setcolor(GC_RMW |(~(activeplane) & 0x0f),15)
#define grcgPlaneRev(activeplane)	grcg_setcolor(GC_RMW |(~(activeplane) & 0x0f),0)
#define grcgPlaneColor(activeplane,color)	grcg_setcolor(GC_RMW |(~(activeplane) & 0x0f),(color))
#define	grcgOff()					grcg_off()
#define grcgPlaneTDW(activeplane)	grcg_setcolor(GC_TDW |(~(activeplane) & 0x0f),15)
#define grcgPlaneRevTDW(activeplane)	grcg_setcolor(GC_TDW |(~(activeplane) & 0x0f),0)

/*
 *	外字 BFNT 表示
 */
#define gfLoad(fontfile,no)	gaiji_backup(),(gaiji_entry_bfnt(fontfile)?0:DATA_ER)
#define gfStore()		gaiji_backup()
#define gfRestore()		gaiji_restore()
#if 0
int		gfExchange( void );
#endif
#define gfDisp(y,x,attr,str)	gaiji_putsa(x,y,str,attr)
#define	gfDispBig(y,x,attr,str)	gfDisp(y,x,attr,str)		/* 小さいぞ */
#define gfDispChr(y,x,attr,c)	gaiji_putca(x,y,c,attr)
#define gfDispTime(y,x,attr,num)	gaiji_putnia(x,y,num,2,attr)
#define gfDispScore(y,x,attr,num)	gaiji_putnia(x,y,num,5,attr)

/*
 *	外字 + グラフィック単色背景 BFNT 表示
 */
#define gfDispEdg(y,x,attr,col2,str) gfDisp(y,x,attr,str)
#define gfDispSdw(y,x,attr,col2,str) gfDisp(y,x,attr,str)
#define gfDispChrEdg(y,x,attr,col2,c) gfDispChr(y,x,attr,c)
#define gfDispChrSdw(y,x,attr,col2,c) gfDispChr(y,x,attr,c)
#define gfDispScoreEdg(y,x,attr,col2,num)	gfDispScore(y,x,attr,num)
#define gfDispScoreSdw(y,x,attr,col2,num)	gfDispScore(y,x,attr,num)
#if 0
void	gfDispEdg3(	int row, int col, int attribute, int color2, PSTR str );
#endif

/*
 *	グラフィック単色BFNT表示
 */
#define	mfDisp(x,y,color,str)	graph_gaiji_puts((x)*8,y,16,str,color)
#define	mfDispChr(x,y,color,c)	graph_gaiji_putc((x)*8,y,c,color)
#define mfDispEdg(x,y,col,col2,str) mfDisp(x,y,col,str)
#define mfDispSdw(x,y,col,col2,str) graph_gaiji_puts((x)*8+1,(y)+1,16,str,col2),mfDisp(x,y,col,str)
#define mfDispChrEdg(x,y,col,col2,c) mfDispChr(x,y,col,c)
#define mfDispChrSdw(x,y,col,col2,c) graph_gaiji_putc((x)*8+1,(y)+1,c,col2),mfDispChr(x,y,col,c)
#if 0
void	mfDispScoreEdg( int row, int col, int color1, int color2, WORD num);
void	mfDispScoreSdw( int row, int col, int color1, int color2, WORD num);
void	bpDisp( int x, int y, BYTE *str );
void	grDisp( int x, int y, int color, PSTR str );
void	grDispWide( int x, int y, int color, PSTR str );
#endif
#define bpDisp(x,y,str)		graph_gaiji_puts((x)*8,y,16,str,4)

/*
 *	マウス
 */
#define	mouseStart()				mouse_vstart(0,4)
#define mouseStop()					mouse_vend()
#define	mouseSetCsrMode(mode)		((mode) ? (void)cursor_show() : (cursor_hide(),cursor_init()))
#define mouseGetInfo(mi)			mouse_get(mi)
#define	mouseSetInfo(mi)			mouse_cmoveto((mi)->x,(mi)->y)
#define mouseSetArea(x1,y1,x2,y2)	mouse_setrect(x1,y1,x2,y2)
#define mouseSetSpeed(sp)			mouse_setmickey((sp)*4,(sp)*4)	/* ? */
#define mouseSetMask(ox,oy,mask)	cursor_pattern2(ox,oy,4,mask)

/*
 *	EMS
 */
int		emsGetStatus( void );
#define	emsGetUnallocatedPages()	(int)(ems_space()/(16384))
#define emsAllocatePages(pageNum)	ems_allocate((pageNum)*16384L)
#define emsDeallocatePages(handle)	ems_free(handle)
#define emsReallocatePages(handle,pageNum)	ems_reallocate(handle,(pageNum)*16384L)
#define emsMapHandlePage(frame,handle,page)	ems_maphandlepage(frame,handle,page)
#define emsSavePageMap(handle)		ems_savepagemap(handle)
#define	emsRestorePageMap(handle)	ems_restorepagemap(handle)
int		emsGetPhysicalAddress( EMS_PHYS_PAGE frame[4] );
#define	emsSetHandleName(handle,name)	ems_setname(handle,name)

/*
 *	テキスト画面
 */
#define txtStore()					!text_backup(1)
#define txtRestore()				text_restore()
#define txtCls()					text_fillca(0,TX_WHITE)
#define txtClearLine(line)			text_putnsa(0,line,"",80,TX_WHITE)
#define	txtDisp(row,col,attr,str)	text_putsa(col,row,str,attr)
#define	txtDispChr(row,col,attr,c)	text_putca(col,row,c,attr)

/*
 *	文字入出力
 */
#define fastPuts(str)	dos_cputs2(str)
#define fastPutc(c)		(((c) == '\n') ? (dos_putch('\r'),dos_putch('\n')) : dos_putch(c))
#define conPuts(str)	dos_puts2(str)
#define conPutc(c)		(((c) == '\n') ? (dos_putc('\r'),dos_putc('\n')) : dos_putc(c))
#define	GetKey()		key_wait()
#define	ScanKey()		(key_pressed() ? key_scan() : 0)
#define	grGetKey()		GetKey()
#define grScanKey()		ScanKey()
#define ScanShift()		key_shift()
#define	SetKey()		key_start()
#define ResetKey()		key_end()
#define biosGetKey()	key_wait_bios()
#define biosSenseKey()	key_sense_bios()

/*
 *	その他
 */
#define	SinVal(t)			(SinDeg(t) * 127 / 256)
#define	CosVal(t)			(CosDeg(t) * 127 / 256)
#define	DisableKeyBeep()	key_beep_off()	/* ちょっと意味が違うけど */
#define	EnableKeyBeep()		key_beep_on()
#define	DisableCtrlC()		dos_ignore_break()
#define	EnableCtrlC()		dos_accept_break()

#define strGetEnv(env,buf) (buf ? strcpy(buf, dos_getenv(0,env)) : dos_getenv(0,env))
#define GetMem(bytes)	  hmem_allocbyte(bytes)
#define GetLargeMem(para) hmem_lallocate((long)(para) << 4)
#define FreeMem(seg)      hmem_free(seg)

#define GetDosVersion()				(((unsigned)_osmajor << 8) | _osminor)
#define FindFirst(path,attr,finfo)	_dos_findfirst(path,attr,finfo)
#define FindNext(finfo)				_dos_findnext(finfo)

/*
 *	変数
 */
#define	GDCMODE		((unsigned char *)&graph_VramZoom)[1]
#define grPalDefault PalettesInit

/*
 * その他
 */
#define cputs(s)	dos_cputs(s)

#endif
