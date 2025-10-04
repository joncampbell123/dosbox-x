/* master.lib		98,AT雑用ライブラリ
 * Version 0.23
 * Copyright (C) 1992-95 A.Koizuka, Kazumi
 *
 * AT互換機でしか使わない場合は、#includeする前に
 *	#define MASTER98 0	(またはコンパイルオプションで -DMASTER98=0)
 * を定義してください。一部関数が98用の名前のまま、AT互換機で利用できます。
 *
 */
#ifndef __MASTER_H
#define __MASTER_H
#define __MASTER_VERSION (0*256+23)

#ifdef MASTER_DOSV
# define MASTERV	1
# define MASTER98	0
#else
# ifndef MASTERV
#  define MASTERV	1	/* AT互換機用機能を使用する */
# endif
# ifndef MASTER98
#  define MASTER98	1	/* 98用機能を使用する */
# endif
# if MASTERV
#  if !MASTER98
#    define MASTER_DOSV 1	/* DOS/V専用なら定義する */
#  endif
# endif
#endif

#if !defined(MASTER_NEAR) && !defined(MASTER_FAR) && !defined(MASTER_COMPACT) && !defined(MASTER_MEDIUM)
#  if defined(__SMALL__) || defined(__TINY__) || defined(M_I86SM) || defined(M_I86TM)
#    define MASTER_NEAR
#  elif defined(__COMPACT__) || defined(M_I86CM)
#    define MASTER_COMPACT
#  elif defined(__MEDIUM__) || defined(M_I86MM)
#    define MASTER_MEDIUM
#  elif defined(__LARGE__) || defined(__HUGE__) || defined(M_I86LM) || defined(M_I86HM)
#    define MASTER_FAR
#  endif
#endif

#ifdef MASTER_NEAR
#  define MASTER_RET near pascal
#  define MASTER_CRET near cdecl
#  define MASTER_PTR near
#endif
#ifdef MASTER_FAR
#  define MASTER_RET far pascal
#  define MASTER_CRET far cdecl
#  define MASTER_PTR far
#endif
#ifdef MASTER_COMPACT
#  define MASTER_RET near pascal
#  define MASTER_CRET near cdecl
#  define MASTER_PTR far
#endif
#ifdef MASTER_MEDIUM
#  define MASTER_RET far pascal
#  define MASTER_CRET far cdecl
#  define MASTER_PTR near
#endif

#ifndef MASTER_RET
#  error master.lib:モデルが特定できません。 master.h を直接includeせずに、masters.h などのモデル別ヘッダをインクルードしてください!
#endif

#ifndef EXIT_SUCCESS	/* stdlib.h は EXIT_SUCCESS で2重includeを防止できる */
# include <stdlib.h>
#endif
#ifndef FP_SEG					/* dos.h は FP_SEG で2重includeを防止できる */
# include <dos.h>
#endif
#ifndef __TURBOC__
# include <conio.h>				/* うーむ。2重includeを防止してない */
#endif
#include <string.h>				/* これもだ */

								/* stdio.hは getchar で二重includeは防止 */
								/* できるよ。防止すると、コンパイルが少し */
								/* 速くなるよね。(エラー回避の効能もある) */

#ifndef OUTB
# ifdef __TURBOC__
#  define OUTB outportb			/* CONIO.H or DOS.H */
#  define OUTW outport
#  define INPB inportb
#  define INPW inport
# else
#  define OUTB outp				/* CONIO.H */
#  define OUTW outpw
#  define INPB inp
#  define INPW inpw
# endif
#endif

#ifndef STI
# ifdef __TURBOC__
#  define STI()	enable()		/* DOS.H */
#  define CLI()	disable()
# else
#  define STI()	_enable()		/* DOS.H */
#  define CLI()	_disable()
# endif
#endif

#define CRLF	"\r\n"

#ifdef __cplusplus
extern "C" {
#endif

extern const char near Master_Version_NEAR[] ;
extern const char near Master_Version_FAR[] ;
extern const char near Master_Version_COMPACT[] ;
extern const char near Master_Version_MEDIUM[] ;
extern const char near Master_Version[] ;

#ifdef MASTER_NEAR
# define	Master_Version_String	Master_Version_NEAR
#endif
#ifdef MASTER_FAR
# define	Master_Version_String	Master_Version_FAR
#endif
#ifdef MASTER_COMPACT
# define	Master_Version_String	Master_Version_COMPACT
#endif
#ifdef MASTER_MEDIUM
# define	Master_Version_String	Master_Version_MEDIUM
#endif

extern unsigned char table_hreverse[256] ;

typedef struct Point Point ;
struct Point {
#ifdef __cplusplus
	Point(int ix,int iy) : x(ix),y(iy) {}
	Point() {}
	void moveto(int nx,int ny) { x = nx, y = ny ; }
#endif
	int x, y ;
} ;

/* 機種判定 ----------------------------------------------------------------*/

typedef struct VIDEO_STATE VIDEO_STATE ;
struct VIDEO_STATE {
	unsigned char mode ;		/* 現在のビデオ・モード */
	unsigned char rows ;		/* 現在の１画面の行数 */
	unsigned char cols ;		/* 現在の１行あたりの桁数 */
	unsigned char total_rows  ;	/* 現在の画面全体の行数 */
} ;

extern const unsigned Machine_State ;

unsigned MASTER_RET get_machine(void);
unsigned MASTER_RET get_machine_at(void);
unsigned MASTER_RET get_machine_98(void);
int MASTER_RET set_video_mode( unsigned video );
unsigned MASTER_RET get_video_mode(void);
void MASTER_RET backup_video_state( VIDEO_STATE MASTER_PTR * vmode );
int MASTER_RET restore_video_state( const VIDEO_STATE MASTER_PTR * vmode );

#define PC_AT			0x0010
#define PC9801			0x0020
#define FMR				0x0080	/* 0.23追加 */
#define DOSBOX			0x8000	/* 0.22k追加 */

#define DESKTOP			0x0001
#define EPSON			0x0002
#define PC_MATE			0x0004
#define HIRESO			0x0008

#define LANG_US			0x0001
#define PC_TYPE_MASK	0x000e
#define PS55			0x0000
#define DOSV			0x0002
#define PC_AX			0x0004
#define J3100			0x0006
#define DR_DOS			0x0008
#define MSDOSV			0x000a
#define VTEXT			0x0240	/* 0.23追加 */
#define DOSVEXTENTION	0x0040	/* 0.22d追加 */
#define SUPERDRIVERS	0x0200	/* 0.23追加 */
#define ANSISYS			0x0100	/* 0.22d追加 */

unsigned MASTER_RET get_cpu(void);
#define CPU_CYRIX		0x4000
#define CPU_NEC			0x8000
#define CPU_V86MODE		0x0100
#define CPU_TYPEMASK	0x00ff
#define CPU_TYPE086		0x0000
#define CPU_TYPE186		0x0001
#define CPU_TYPE286		0x0002
#define CPU_TYPE386		0x0003
#define CPU_TYPE486		0x0004
#define CPU_TYPEPENTIUM	0x0005

/* Cx486関係 ---------------------------------------------------------------*/

struct Cx486CacheInfo {
	unsigned ccr0 ;
	unsigned ccr1 ;
	unsigned long ncr[4] ;
} ;

extern void MASTER_RET cx486_read( struct Cx486CacheInfo MASTER_PTR * rec );
extern void MASTER_RET cx486_write( const struct Cx486CacheInfo MASTER_PTR * rec );
extern void MASTER_RET cx486_cacheoff(void);


/* テキスト画面関係 --------------------------------------------------------*/

extern void far * const TextVramAdr ;
extern unsigned TextVramSeg ;
extern unsigned TextVramWidth ;
extern unsigned TextShown ;

extern unsigned TextVramSize ;
extern unsigned VTextState ;

#define TX_GETSIZE(x1,y1,x2,y2)	(((x2)-(x1)+1)*((y2)-(y1)+1)*TX_GET_UNIT)
void MASTER_RET text_roll_area( int x1, int y1, int x2, int y2 );

#if MASTER98

void MASTER_RET text_start(void);
void MASTER_RET text_end(void);
void MASTER_RET text_boxfilla( unsigned x1, unsigned y1, unsigned x2, unsigned y2, unsigned atrb );
void MASTER_RET text_boxfillca( unsigned x1, unsigned y1, unsigned x2, unsigned y2, unsigned fillchar, unsigned atrb );
void MASTER_RET text_clear(void);
void MASTER_RET text_fillca( unsigned ch, unsigned atrb );
void MASTER_RET text_locate( unsigned x, unsigned y );
void MASTER_RET text_putc( unsigned x, unsigned y, unsigned ch );
void MASTER_RET text_putca( unsigned x, unsigned y, unsigned ch, unsigned atrb );
void MASTER_RET text_putns( unsigned x, unsigned y, const char MASTER_PTR *str, unsigned wid );
void MASTER_RET text_putnp( unsigned x, unsigned y, const char MASTER_PTR *pstr, unsigned wid );
void MASTER_RET text_putnsa( unsigned x, unsigned y, const char MASTER_PTR *str, unsigned wid, unsigned atrb );
void MASTER_RET text_putnpa( unsigned x, unsigned y, const char MASTER_PTR *pstr, unsigned wid, unsigned atrb );
void MASTER_RET text_puts( unsigned x, unsigned y, const char MASTER_PTR *str );
void MASTER_RET text_putp( unsigned x, unsigned y, const char MASTER_PTR *pstr );
void MASTER_RET text_putsa( unsigned x, unsigned y, const char MASTER_PTR *str, unsigned atrb );
void MASTER_RET text_putpa( unsigned x, unsigned y, const char MASTER_PTR *pstr, unsigned atrb );
void MASTER_RET text_vputs(unsigned x, unsigned y, const char MASTER_PTR *str);
void MASTER_RET text_vputsa(unsigned x, unsigned y, const char MASTER_PTR *str, unsigned atrb);

void MASTER_RET text_setcursor( int normal );

#define text_width()	80
int MASTER_RET text_height(void);
int MASTER_RET text_systemline_shown(void);
void MASTER_RET text_25line(void);
void MASTER_RET text_20line(void);
void MASTER_RET text_cursor_hide(void);
void MASTER_RET text_cursor_show(void);
void MASTER_RET _text_cursor_on(void);
void MASTER_RET _text_cursor_off(void);
void MASTER_RET text_systemline_hide(void);
void MASTER_RET text_systemline_show(void);
long MASTER_RET text_getcurpos(void);
void MASTER_RET text_get( int x1,int y1, int x2,int y2, void far *buf );
void MASTER_RET text_put( int x1,int y1, int x2,int y2, const void far *buf );
int MASTER_RET text_backup( int use_main );
int MASTER_RET text_restore(void);
void MASTER_RET text_show(void);
void MASTER_RET text_hide(void);
void MASTER_RET text_smooth_start( unsigned y1, unsigned y2 );
void MASTER_RET text_smooth_end(void);
void MASTER_RET text_roll_up_c( unsigned fillchar );
void MASTER_RET text_roll_down_c( unsigned fillchar );
void MASTER_RET text_roll_up_ca( unsigned fillchar, unsigned filatr );
void MASTER_RET text_roll_down_ca( unsigned fillchar, unsigned filatr );
void MASTER_RET text_roll_left_c( unsigned fillchar );
void MASTER_RET text_roll_right_c( unsigned fillchar );
void MASTER_RET text_roll_left_ca( unsigned fillchar, unsigned filatr );
void MASTER_RET text_roll_right_ca( unsigned fillchar, unsigned filatr );
void MASTER_RET text_pseta( int x, int y, unsigned atr );
void MASTER_RET text_pset( int x, int y );
void MASTER_RET text_preset( int x, int y );
void MASTER_RET text_worddota( int x, int y, unsigned image, unsigned dotlen, unsigned atr );
void MASTER_RET text_showpage( int page );
void MASTER_RET text_frame( int x1, int y1, int x2,int y2, unsigned wattr, unsigned iattr, int round ); 

#define text_shown()	TextShown
#define text_smooth(shiftdot)	OUTB(0x76,shiftdot)
#define text_vertical()			OUTB(0x68,0)
#define text_cemigraph()		OUTB(0x68,1)
#define text_cursor_shown()		(*(char far *)0x071bL)

#define TX_PAGE0		0xa000	/* ノーマルモードのテキストセグメント */
#define TX_PAGE1		0xa100
#define text_accesspage(p)	(TextVramSeg = (p) ? TX_PAGE1 : TX_PAGE0 )

#endif

#define TX_BLACK		0x01	/* テキスト属性 */
#define TX_BLUE			0x21
#define TX_RED			0x41
#define TX_MAGENTA		0x61
#define TX_GREEN		0x81
#define TX_CYAN			0xa1
#define TX_YELLOW 		0xc1
#define TX_WHITE		0xe1
#define TX_BLINK		2
#define TX_REVERSE		4
#define TX_UNDERLINE	8

#define TX_GET_UNIT		4 /* text_get()で退避する、1文字あたり必要な大きさ */

  /* 前バージョンとの互換性のため */
#define text_cursor_on()		text_cursor_show()
#define text_cursor_off()		text_cursor_hide()
#define text_systemline_on()	text_systemline_show()
#define text_systemline_off()	text_systemline_hide()

#if MASTERV

extern char vtext_colortable[16] ;
extern unsigned vtextx_Seg, vtextx_Size ;

void MASTER_RET vtext_start(void);
#define vtext_end()		/* ignore */
void MASTER_RET vtextx_start(void);
void MASTER_RET vtextx_end(void);
int MASTER_RET vtext_height(void);
int MASTER_RET vtext_systemline_shown(void);
int MASTER_RET vtext_width(void);
int MASTER_RET vtext_font_height(void);
void MASTER_RET vtext_systemline_hide(void);
void MASTER_RET vtext_systemline_show(void);
long MASTER_RET vtext_getcurpos(void);
void MASTER_RET vtext_locate( unsigned x, unsigned y );
void MASTER_RET vtext_putns( unsigned x, unsigned y, const char MASTER_PTR *str, unsigned wid );
void MASTER_RET vtext_putnsa( unsigned x, unsigned y, const char MASTER_PTR *str, unsigned wid, unsigned atrb );
void MASTER_RET vtext_putsa( unsigned x, unsigned y, const char MASTER_PTR *strp, unsigned atrb );
void MASTER_RET vtext_puts( unsigned x, unsigned y, const char MASTER_PTR *strp );
void MASTER_RET vtext_putc( unsigned x, unsigned y, unsigned ch );
void MASTER_RET vtext_putca( unsigned x, unsigned y, unsigned ch, unsigned atrb );
void MASTER_RET vtext_roll_up_c( unsigned fillchar );
void MASTER_RET vtext_roll_down_c( unsigned fillchar );
void MASTER_RET vtext_roll_up_ca( unsigned fillchar, unsigned filatr );
void MASTER_RET vtext_roll_down_ca( unsigned fillchar, unsigned filatr );

void MASTER_RET vtext_clear(void);
void MASTER_RET vtext_frame(int x1, int y1, int x2, int y2, unsigned attrl, unsigned attri, int dummy); 
void MASTER_RET vtext_boxfilla(int x1, int y1, int x2, int y2, unsigned attr ); 

int MASTER_RET vtext_color_98( int color98 );
void MASTER_RET vtext_refresh(unsigned x, unsigned y, unsigned len);
void MASTER_RET vtext_refresh_all(void);
void MASTER_RET vtext_refresh_on(void);
void MASTER_RET vtext_refresh_off(void);
void MASTER_RET vtext_setcursor( unsigned cursor );
unsigned MASTER_RET vtext_getcursor(void);
# define vtext_cursor_hide()		vtext_setcursor(vtext_getcursor()|0x2000)
# define vtext_cursor_show()		vtext_setcursor(vtext_getcursor()&~0x2000)
# define vtext_cursor_shown()		(((vtext_getcursor()>>8)&0x3f) \
									 <= (vtext_getcursor()&7))
void MASTER_RET vtext_get( int x1,int y1, int x2,int y2, void far *buf );
void MASTER_RET vtext_put( int x1,int y1, int x2,int y2, const void far *buf );
int MASTER_RET vtext_backup( int use_main );
int MASTER_RET vtext_restore(void);


# if !MASTER98
#  define text_start()			(get_machine(),vtext_start(),font_at_init())
#  define text_end()							(void)0
#  define text_boxfilla(x1,y1,x2,y2,atrb)		vtext_boxfilla(x1,y1,x2,y2,\
												vtext_color_98(atrb))
#  define text_boxfillca(x1,y1,x2,y2,fc,atrb)	/* ? */
#  define text_clear()							vtext_clear()
#  define text_fillca(ch,atrb)					/* ? */
#  define text_locate(x,y)						vtext_locate(x,y)
#  define text_putc(x,y,ch)						vtext_putc(x,y,ch)
#  define text_putca(x,y,ch,atrb)				vtext_putca(x,y,ch,\
													vtext_color_98(atrb))
#  define text_putns(x,y,str,wid)				vtext_putns(x,y,str,wid)
#  define text_putnp(x,y,pstr,wid)				/* ? */
#  define text_putnsa(x,y,str,wid,atrb)			vtext_putnsa(x,y,str,wid,\
													vtext_color_98(atrb))
#  define text_putnpa(x,y,pstr,wid,atrb)		/* ? */
#  define text_puts(x,y,str)					vtext_puts(x,y,str)
#  define text_putp(x,y,pstr)					/* ? */
#  define text_putsa(x,y,str,atrb)				vtext_putsa(x,y,str,\
													vtext_color_98(atrb))
#  define text_putpa(x,y,pstr,atrb)				/* ? */
#  define text_vputs(x,y,str)					/* ? */
#  define text_vputsa(x,y,str,atrb)				/* ? */

#  define text_setcursor(normal)				vtext_setcursor(normal)

#  define text_width()							vtext_width()
#  define text_height()							vtext_height()
#  define text_systemline_shown()				vtext_systemline_shown()
#  define text_25line()							/* ? */
#  define text_20line()							/* ? */
#  define text_cursor_hide()					vtext_cursor_hide()
#  define text_cursor_show()					vtext_cursor_show()
#  define _text_cursor_on()						text_cursor_show()
#  define _text_cursor_off()					text_cursor_hide()
#  define text_systemline_hide()				vtext_systemline_hide()
#  define text_systemline_show()				vtext_systemline_show()
#  define text_getcurpos()						vtext_getcurpos()
#  define text_get(x1,y1,x2,y2,buf)				vtext_get(x1,y1,x2,y2,buf)
#  define text_put(x1,y1,x2,y2,buf)				vtext_put(x1,y1,x2,y2,buf)
#  define text_backup(use_main)					(text_start(),vtext_backup(use_main))
#  define text_restore()						vtext_restore()
#  define text_show()							(void)0
#  define text_hide()							(void)0
#  define text_smooth_start(y1,y2)				(void)0
#  define text_smooth_end()						(void)0
#  define text_roll_up_c(fillchar)				vtext_roll_up_c(fillchar)
#  define text_roll_down_c(fillchar)			vtext_roll_down_c(fillchar)
#  define text_roll_up_ca(fillchar,atrb)		vtext_roll_up_ca(fillchar,\
												vtext_color_98(atrb))
#  define text_roll_down_ca(fillchar,atrb)	vtext_roll_down_ca(fillchar,\
												vtext_color_98(atrb))
#  define text_roll_left_c(fillchar)			/* ? */
#  define text_roll_right_c(fillchar)			/* ? */
#  define text_roll_left_ca(fillchar,atrb)		/* ? */
#  define text_roll_right_ca(fillchar,atrb)		/* ? */
#  define text_pseta(x,y,atr)					(void)0
#  define text_pset(x,y)						(void)0
#  define text_preset(x,y)						(void)0
#  define text_worddota(x,y,image,dotlen,atr)	(void)0
#  define text_showpage(page)					(void)0
#  define text_frame(x1,y1,x2,y2,wattr,iattr,round)	vtext_frame(x1,y1,x2,y2,\
							vtext_color_98(wattr),vtext_color_98(iattr),round)

#  define text_shown()					1
#  define text_smooth(shiftdot)			(void)0
#  define text_vertical()				(void)0
#  define text_cemigraph()				(void)0
#  define text_cursor_shown()			vtext_cursor_shown()

#  define TX_PAGE0		TextVramSeg
#  define TX_PAGE1		TextVramSeg
#  define text_accesspage(p)			(void)0
# endif
#endif

/* EMS関係 -----------------------------------------------------------------*/

#if defined(_MSC_VER)
# pragma pack (1)
#endif
struct EMS_move_source_dest {
	long region_length ;
	char source_memory_type ;
	unsigned source_handle ;
	unsigned source_initial_offset ;
	unsigned source_initial_seg_page ;
	char dest_memory_type ;
	unsigned dest_handle ;
	unsigned dest_initial_offset ;
	unsigned dest_initial_seg_page ;
} ;
#if defined(_MSC_VER)
# pragma pack ()
#endif

int MASTER_RET ems_exist(void);
unsigned MASTER_RET ems_allocate( unsigned long len );
int MASTER_RET ems_reallocate( unsigned handle, unsigned long size );
int MASTER_RET ems_free( unsigned handle );
unsigned long MASTER_RET ems_size( unsigned handle );
int MASTER_RET ems_setname( unsigned handle, const char MASTER_PTR * name );
int MASTER_RET ems_movememoryregion( const struct EMS_move_source_dest MASTER_PTR * block );
void MASTER_RET ems_enablepageframe( int enable );

int MASTER_RET ems_write( unsigned handle, long offset, const void far * mem, long size );
int MASTER_RET ems_read( unsigned handle, long offset, void far * mem, long size );
unsigned long MASTER_RET ems_space(void);
int MASTER_RET ems_maphandlepage( int phys_page, unsigned handle, unsigned log_page );
int MASTER_RET ems_savepagemap( unsigned handle );
int MASTER_RET ems_restorepagemap( unsigned handle );
int MASTER_RET ems_getsegment( unsigned MASTER_PTR * segments, int maxframe );
unsigned MASTER_RET ems_findname( const char MASTER_PTR * hname );

long MASTER_RET ems_dos_read( int file_handle, unsigned short ems_handle, unsigned long ems_offs, long read_bytes );
long MASTER_RET ems_dos_write( int file_handle, unsigned short ems_handle, unsigned long ems_offs, long write_bytes );

/* グラフィック画面関係 ----------------------------------------------------*/

typedef struct PiHeader PiHeader ;
struct PiHeader {
	char far *comment;		/* graph_pi_load.*()では */
							/* NULLが設定されるだけ */
	unsigned commentlen;
	unsigned char mode;
	unsigned char n;		/* aspect */
	unsigned char m;		/* aspect */
	unsigned char plane;	/* 通常は 4 */
	char machine[4];
	unsigned maexlen;		/* machine extend data length */
	void far * maex;		/* machine extend data */
	unsigned xsize;
	unsigned ysize;
	unsigned char palette[48];
} ;

typedef struct MagHeader MagHeader;
struct MagHeader {
	unsigned commentseg ;
	unsigned commentlen ;
	char head;
	char machine;
	char exflag;
	char scrnmode;
	int x1;
	int y1;
	int x2;
	int y2;
	long flagAofs;
	long flagBofs;
	long flagBsize;
	long pixelofs;
	long pixelsize;
	unsigned xsize;
	unsigned ysize;
	unsigned char palette[48];
} ;

#if MASTER98
void MASTER_RET graph_400line(void);
void MASTER_RET graph_200line( int tail );
unsigned MASTER_RET graph_extmode( unsigned modmask, unsigned bhal );
#define graph_is31kHz()		((graph_extmode(0,0) & 0x0c) == 0x0c)
#define graph_getextmode()	graph_extmode(0,0)
#define graph_setextmode(v)	graph_extmode(0xffff,(v))
#define graph_31kHz()		graph_extmode(0x0c,0x0c)
#define graph_24kHz()		graph_extmode(0x0c,0x08)
#define graph_480line()		graph_extmode(0x300c,0x300c)
int MASTER_RET graph_is256color(void);
void MASTER_RET graph_256color(void);
void MASTER_RET graph_16color(void);
void MASTER_RET graph_clear(void);
void MASTER_RET graph_show(void);
void MASTER_RET graph_hide(void);
void MASTER_RET graph_start(void);
void MASTER_RET graph_end(void);
void MASTER_RET graph_enter(void);
#define graph_leave() respal_set_palettes()
void MASTER_RET graph_xlat_dot( int x, int y, const char MASTER_PTR * trans );
int MASTER_RET graph_shown(void);
int MASTER_RET graph_readdot( int x, int y );
int MASTER_RET graph_backup( int pagemap );
int MASTER_RET graph_restore(void);
void MASTER_RET graph_xorboxfill( int x1,int y1, int x2,int y2, int color );
int MASTER_RET graph_copy_page( int to_page );
void MASTER_RET graph_scroll( unsigned line1, unsigned adr1, unsigned adr2 );
void MASTER_RET graph_scrollup( unsigned line );
void MASTER_RET graph_byteget( int cx1,int y1, int cx2,int y2, void far *buf );
void MASTER_RET graph_byteput( int cx1,int y1, int cx2,int y2, const void far *buf );
void MASTER_RET graph_move( int x1,int y1, int x2,int y2, int nx,int ny );
void MASTER_RET graph_pack_put_8( int x, int y, const void far * linepat, int len );
void MASTER_RET graph_pack_put_down_8( int x, int y, const void far * pat, int patwidth, int width, int height );
void MASTER_RET graph_pack_get_8( int x, int y, void far * linepat, int len );
void MASTER_RET graph_unpack_put_8( int x, int y, const void far * linepat, int len );
void MASTER_RET graph_unpack_large_put_8( int x, int y, const void far * linepat, int len );
void MASTER_RET graph_unpack_get_8( int x, int y, void far * linepat, int len );

void MASTER_RET graph_ank_putc(int x, int y, int ch, int color);
#define graph_ank_put(x,y,chp,color) graph_ank_putc((x),(y),*(chp),(color))
void MASTER_RET graph_ank_puts(int x, int y, int step, const char MASTER_PTR * ank, int color);
void MASTER_RET graph_ank_putp(int x, int y, int step, const char MASTER_PTR * passtr, int color);
void MASTER_RET graph_font_put(int x, int y, const char MASTER_PTR *str, int color);
void MASTER_RET graph_font_puts(int x, int y, int step, const char MASTER_PTR * str, int color);
void MASTER_RET graph_font_putp(int x, int y, int step, const char MASTER_PTR * passtr, int color);
void MASTER_RET graph_kanji_large_put(int x, int y, const char MASTER_PTR *str, int color);
void MASTER_RET graph_kanji_put(int x, int y, const char MASTER_PTR *str, int color);
void MASTER_RET graph_kanji_puts(int x, int y, int step, const char MASTER_PTR *str, int color);

void MASTER_RET graph_wank_putc(int x, int y, int c );
void MASTER_RET graph_wank_puts(int x, int y, int step, const char MASTER_PTR * str );
void MASTER_RET graph_wank_putca(int x, int y, int ch, int color);
void MASTER_RET graph_wank_putsa(int x, int y, int step, const char MASTER_PTR * str, int color);
void MASTER_RET graph_wfont_put(int x, int y, const char MASTER_PTR * str);
void MASTER_RET graph_wfont_puts(int x, int y, int step, const char MASTER_PTR * str);
void MASTER_RET graph_wkanji_put(int x, int y, const char MASTER_PTR * str);
void MASTER_RET graph_wkanji_put_left(int x, int y, const char MASTER_PTR * str);
void MASTER_RET graph_wkanji_put_right(int x, int y, const char MASTER_PTR * str);
void MASTER_RET graph_wkanji_puts(int x, int y, int step, const char MASTER_PTR * str);
void MASTER_RET graph_gaiji_putc(int x, int y, int c, int color );
void MASTER_RET graph_gaiji_puts(int x, int y, int step, const char MASTER_PTR * str, int color );
void MASTER_RET graph_bfnt_puts(int x, int y, int step, const char MASTER_PTR * str, int color);
void MASTER_RET graph_bfnt_putp(int x, int y, int step, const char MASTER_PTR * passtr, int color);
void MASTER_RET graph_bfnt_putc(int x, int y, int ank, int color);
#endif

#if MASTERV
void MASTER_RET vga4_clear(void);
int MASTER_RET vga4_start(int videomode, int xdots, int ydots);
#define vgc_start()			vga4_start(0x12,640,480)
#define vgc_end()			vga4_end()
void MASTER_RET vga4_end(void);
int MASTER_RET vga4_readdot(int x,int y);
void MASTER_RET vga4_byteget( int cx1,int y1, int cx2,int y2, void far *buf );
void MASTER_RET vga4_byteput( int cx1,int y1, int cx2,int y2, const void far *buf );
void MASTER_RET vga4_pack_put_8( int x, int y, const void far * linepat, int len );
void MASTER_RET vga4_unpack_put_8( int x, int y, const void far * linepat, int len );
void MASTER_RET vga4_unpack_get_8( int x, int y, void far * linepat, int len );
void MASTER_RET vga4_pack_get_8( int x, int y, void far * linepat, int len );
void MASTER_RET vga_dc_modify( int num, int andval, int orval );
void MASTER_RET vga_startaddress( unsigned address );
void MASTER_RET vga_setline( unsigned lines );
#define vga_vzoom_on()		(vga_dc_modify(0x09,0x7f,0x80),graph_VramZoom=1)
#define vga_vzoom_off()		(vga_dc_modify(0x09,0x7f,0x00),graph_VramZoom=0)
void MASTER_RET vga4_byte_move(int x1,int y1,int x2,int y2,int tox,int toy);

void MASTER_RET vga4_wfont_puts(int x, int y, int step, const char MASTER_PTR * str);
void MASTER_RET vga4_bfnt_puts( int x, int y, int step, const char MASTER_PTR * anks, int color );
void MASTER_RET vga4_bfnt_putc( int x, int y, int c, int color );

void MASTER_RET at98_graph_400line(void);
void MASTER_RET at98_accesspage( int page );
void MASTER_RET at98_showpage( int page );
void MASTER_RET at98_scroll( unsigned line1, unsigned adr1 );
extern int at98_APage, at98_VPage ;
extern unsigned at98_Offset ;

# if !MASTER98
#  define graph_analog()	(void)0
#  define graph_digital()	(void)0/* ignore? */
#  define graph_plasma()	(void)0
#  define graph_crt()		(void)0
#  define graph_accesspage(p) at98_accesspage(p)
#  define graph_showpage(p)	at98_showpage(p)
#  define graph_400line()	at98_graph_400line()
#  define graph_200line(t)	(vga4_start(0x8e,640,200),\
		graph_VramSeg=ClipYT_seg=(0xa000+((t)&1)*400*5),at98_Offset=0,\
		at98_showpage(at98_VPage),at98_accesspage(at98_APage),vtextx_start())
#  define graph_is31kHz()	0
#  define graph_getextmode()	0
#  define graph_setextmode(v)	(void)0
#  define graph_31kHz()		(void)0
#  define graph_24kHz()		(void)0
#  define graph_480line()	(void)0
#  define graph_is256color() 0
#  define graph_16color()   (void)0
#  define graph_clear()		vga4_clear()
#  define graph_show()		(void)0
#  define graph_hide()		(void)0
#  define graph_start()		(vga4_start(0x0e,640,400),vga_vzoom_off(),\
							at98_Offset=at98_APage=at98_VPage=0,vtextx_start())
#  define graph_end()		(vtextx_end(),vga4_end())
#  define graph_enter()		graph_400line()
#  define graph_leave()		graph_end()
#  define graph_xlat_dot(x,y,tr)	vgc_setcolor(VGA_PSET,tr[vga4_readdot(x,y)]),vgc_pset(x,y)
#  define graph_shown()		(get_video_mode() == 0x0e)
#  define graph_readdot(x,y) vga4_readdot(x,y)
#  define graph_backup(pm)	1
#  define graph_restore()	1
#  define graph_xorboxfill(x1,y1,x2,y2,c)	(vgc_setcolor(VGA_XOR,c),\
							vgc_boxfill(x1,y1,x2,y2),vgc_setcolor(VGA_PSET,0))
#  define graph_copy_page(to)	(graph_accesspage(0),\
				vga4_byte_move(0,(to)?0:400,79,(to)?399:799,0,(to)?400:0),\
								graph_accesspage(to),1)
#  define graph_scroll(line1,adr1,adr2)		at98_scroll(line1,adr1)
#  define graph_scrollup(up)				at98_scroll(graph_VramLines-(up),\
											(up)*graph_VramWidth/2)
#  define graph_byteget(cx1,y1,cx2,y2,b)	vga4_byteget(cx1,y1,cx2,y2,b)
#  define graph_byteput(cx1,y1,cx2,y2,b)	vga4_byteput(cx1,y1,cx2,y2,b)
#  define graph_pack_put_8(x,y,linepat,len)	vga4_pack_put_8(x,y,linepat,len)
#  define graph_unpack_large_put_8(x,y,linepat,len)	vga4_pack_put_8(x,y,linepat,(len)*2)/* ? */
#  define graph_pack_get_8(x,y,buf,len)	vga4_pack_get_8(x,y,buf,len)
#  define graph_unpack_put_8(x,y,linepat,len)	vga4_unpack_put_8(x,y,linepat,len)
#  define graph_unpack_get_8(x,y,buf,len)	vga4_unpack_get_8(x,y,buf,len)
#  define graph_wfont_puts(x,y,s,str)		vga4_wfont_puts(x,y,s,str)
#  define graph_bfnt_puts(x,y,s,str,c)	(vgc_setcolor(VGA_PSET,c),vgc_bfnt_puts(x,y,s,str))
#  define graph_bfnt_putc(x,y,ank,c)	(vgc_setcolor(VGA_PSET,c),vgc_bfnt_putc(x,y,ank))
#  define graph_gaiji_puts(x,y,s,str,c)	graph_bfnt_puts(x,y,s,str,c) /* ? */
#  define graph_gaiji_putc(x,y,ank,c)	graph_bfnt_putc(x,y,ank,c)   /* ? */
#  define graph_ank_puts(x,y,s,str,c)	graph_bfnt_puts(x,y,s,str,c)
#  define graph_ank_putc(x,y,ank,c)		graph_bfnt_putc(x,y,ank,c)
#  define graph_font_puts(x,y,s,str,c)	(vgc_setcolor(VGA_PSET,c),vgc_font_puts(x,y,s,str))
#  define graph_font_put(x,y,str,c)		(vgc_setcolor(VGA_PSET,c),vgc_font_put(x,y,str))
#  define graph_kanji_put(x,y,str,c)	(vgc_setcolor(VGA_PSET,c),vgc_kanji_putc(x,y,_rotr(*(unsigned short far *)(str),8)))
#  define graph_kanji_puts(x,y,s,str,c)	(vgc_setcolor(VGA_PSET,c),vgc_kanji_puts(x,y,s,str))
#  define graph_kanji_putc(x,y,knj,c)	(vgc_setcolor(VGA_PSET,c),vgc_kanji_putc(x,y,knj))

# endif
#endif

int MASTER_RET mag_load_pack( const char MASTER_PTR * filename, MagHeader MASTER_PTR * header, void far * MASTER_PTR * bufptr );
int MASTER_RET graph_pi_load_pack(const char MASTER_PTR * filename, PiHeader MASTER_PTR * header, void far * MASTER_PTR * bufptr );
void far * MASTER_RET graph_pi_load_unpack(const char MASTER_PTR * filename, PiHeader MASTER_PTR * header );
int MASTER_RET graph_pi_comment_load(const char MASTER_PTR * filename, PiHeader MASTER_PTR * header );
void MASTER_RET graph_pi_free( PiHeader MASTER_PTR * header, const void far * image );
void MASTER_RET mag_free( MagHeader MASTER_PTR * header, const void far * image );

#define graph_wfont_plane(a,b,c)	(wfont_Plane1 = (unsigned char)(a), wfont_Plane2 = (unsigned char)(b), wfont_Reg = (unsigned char)(c))

#if MASTER98

#define graph_analog()	OUTB( 0x6a, 1 )
#define graph_digital()	OUTB( 0x6a, 0 )
#define graph_plasma()	OUTB( 0x6a, 0x41 )
#define graph_crt()		OUTB( 0x6a, 0x40 )
#define graph_showpage(p)	OUTB( 0xa4, p )
#define graph_accesspage(p)	OUTB( 0xa6, p )

#define G_PAGE0		1		/* graph_backup()の退避フラグ */
#define G_PAGE1		2
#define G_ALLPAGE	3


#endif

						/* GB_GETSIZE: graph_byteget()の必要メモリ計算 */
#define GB_GETSIZE(x1,y1,x2,y2)	(((x2)-(x1)+1L)*((y2)-(y1)+1)*4)

extern unsigned graph_VramSeg ;
extern unsigned graph_VramWords ;
extern unsigned graph_VramLines ;
extern unsigned graph_VramWidth ;
extern unsigned graph_VramZoom ;
extern unsigned char graph_MeshByte ;

extern unsigned font_AnkSeg, font_AnkSize, font_AnkPara ;
extern const volatile unsigned font_ReadChar ;
extern const unsigned font_ReadEndChar ;
extern unsigned char wfont_Plane1, wfont_Plane2, wfont_Reg ;
extern unsigned wfont_AnkSeg ;
#if !MASTER98
extern void (far *font_AnkFunc)(void);
extern void (far *font_KanjiFunc)(void);
#endif


/* アナログパレット関係 ----------------------------------------------------*/

#if MASTER98

void MASTER_RET palette_init(void);
void MASTER_RET palette_show(void);
void MASTER_RET palette_show100(void);

#define palette_settone(t)	(PaletteTone = (t),palette_show())
#define palette_100()		palette_settone(100)
#define palette_black()		palette_settone(0)
#define palette_white()		palette_settone(200)

#endif

#define palette_set(n,r,g,b) (Palettes[n][0]=(unsigned char)(r),Palettes[n][1]=(unsigned char)(g),Palettes[n][2]=(unsigned char)(b))
#define palette_set_all(m)		memcpy(Palettes,(m),16*3)
void MASTER_RET palette_set_all_16( const void MASTER_PTR * palette );

#if MASTERV

void MASTER_RET dac_init(void);
void MASTER_RET dac_show(void);

#define dac_settone(t)	(PaletteTone = (t),dac_show())
#define dac_100()		dac_settone(100)
#define dac_black()		dac_settone(0)
#define dac_white()		dac_settone(200)

# if !MASTER98
#  define palette_init()		dac_init()
#  define palette_show()		dac_show()

#  define palette_settone(t)	dac_settone(t)
#  define palette_100()			dac_100()
#  define palette_black()		dac_black()
#  define palette_white()		dac_white()
# endif

#endif

extern unsigned PaletteTone ;
extern int PaletteNote ;
extern const char PalettesInit[16][3] ;
extern unsigned char Palettes[16][3] ;


/* SDI関係 ---------------------------------------------------------------- */
/*					SDI: (c)Ｎｏｂ											*/

#if MASTER98
int MASTER_RET sdi_exist(void);
void MASTER_RET sdi_set_palettes( int page );
void MASTER_RET sdi_get_palettes( int page );

extern int SdiExists ;
#endif

/* 30行BIOS関係 ----------------------------------------------------------- */
/*					30行BIOS: (c)lucifer, walker, さかきけい				*/
/*					TT: (c)「早紀」											*/

typedef struct bios30param bios30param ;
struct bios30param {
	char hs ;
	char vs ;
	char vbp ;
	char vfp ;
	char hbp ;
	char hfp ;
} ;

#define BIOS30_MODEMASK 0xd0d0			/* getmode */
#define BIOS30_NORMAL	0xd000			/* getmode & setmode */
#define BIOS30_SPECIAL	0xd010			/* getmode & setmode */
#define BIOS30_RATIONAL	0xd050			/* getmode & setmode */
#define BIOS30_VGA		0xd090			/* getmode & setmode */
#define BIOS30_LAYER	BIOS30_RATIONAL	/* getmode & setmode */

#define BIOS30_FKEYMASK	0x2020	/* getmode */
#define BIOS30_FKEY		0x2000	/* getmode & setmode */
#define BIOS30_CW		0x2020	/* getmode & setmode */

#define BIOS30_LINEMASK   0x0101	/* getmode */
#define BIOS30_WIDELINE	  0x0101	/* getmode & setmode */
#define BIOS30_NORMALLINE 0x0100	/* getmode & setmode */

#define BIOS30_TT_OLD			0x02
#define BIOS30_30BIOS_OLD		0x80
#define BIOS30_30BIOS_020		0x81
#define BIOS30_30BIOS_120		0x89
#define BIOS30_TT_090			0x83
#define BIOS30_TT_100			0x82
#define BIOS30_TT_105			0x83
#define BIOS30_TT_150			0x06
#define BIOS30_TT				0x02

#define BIOS30_CLOCK2	0	/* getclock & setclock */
#define BIOS30_CLOCK5	1	/* getclock & setclock */

#if MASTER98
int MASTER_RET bios30_tt_exist(void);
#define bios30_exist()	(bios30_tt_exist() >= BIOS30_30BIOS_OLD)
unsigned MASTER_RET bios30_getmode(void);
void MASTER_RET bios30_setmode( unsigned mode );
unsigned MASTER_RET bios30_getversion(void);
int MASTER_RET bios30_push(void);
int MASTER_RET bios30_pop(void);
unsigned MASTER_RET bios30_getline(void);
void MASTER_RET bios30_setline( int lines );
unsigned MASTER_RET bios30_getlimit(void);
int MASTER_RET bios30_getparam( int line, struct bios30param MASTER_PTR * param );

void MASTER_RET bios30_lock(void);
void MASTER_RET bios30_unlock(void);
int MASTER_RET bios30_getclock(void);
int MASTER_RET bios30_setclock( int clock );
unsigned MASTER_RET bios30_getvsync(void);

#endif

#if MASTERV
# if !MASTER98
	/* AT互換機には 30bios/TT なんてないよーん */
#  define bios30_tt_exist()	0
#  define bios30_exist()	0
#  define bios30_getmode	0
#  define bios30_setmode(m)		(void)0
#  define bios30_getversion()	0
#  define bios30_push()			0
#  define bios30_pop()			0
#  define bios30_getline()		0
#  define bios30_setline(l)		(void)0
#  define bios30_getlimit()		(vtext_height()*0x0101)
#  define bios30_getparam(l,p)	0
#  define bios30_lock()			0
#  define bios30_unlock()		0
#  define bios30_getclock()		0
#  define bios30_setclock(c)	(void)0
#  define bios30_getvsync()		0
# endif
#endif

/* RSL(常駐シンボリックリンク)関係 ---------------------------------------- */
/*					RSL: (c)恋塚											*/

#define RSL_NOCONVERT	1
#define RSL_CONVERT		0

int MASTER_RET rsl_exist(void);
int MASTER_RET rsl_readlink( char MASTER_PTR * buf, const char MASTER_PTR * path );
int MASTER_RET rsl_linkmode( unsigned mode );


/* 常駐データ関係 --------------------------------------------------------- */

unsigned MASTER_RET resdata_exist( const char MASTER_PTR * id, unsigned idlen, unsigned parasize );
unsigned MASTER_RET resdata_create( const char MASTER_PTR * id, unsigned idlen, unsigned parasize );
#define resdata_free(seg)	dos_free(seg)

/* 常駐パレット関係 ------------------------------------------------------- */

#if MASTER98

int MASTER_RET respal_exist(void);
int MASTER_RET respal_create(void);
void MASTER_RET respal_get_palettes(void);
void MASTER_RET respal_set_palettes(void);
void MASTER_RET respal_free(void);

extern unsigned ResPalSeg ;

#endif

#if MASTERV
# if !MASTER98
		/* AT互換機では、常駐パレットは作れず、読めないことにする */
#define respal_exist()			0
#define respal_create()			0
#define respal_get_palettes()	(void)0
#define respal_set_palettes()	(void)0
#define respal_free()			(void)0

# endif
#endif

/* ファイル関係 ----------------------------------------------------------- */

int MASTER_RET file_ropen( const char MASTER_PTR * filename );
int MASTER_RET file_read( void far * buf, unsigned wsize );
unsigned long MASTER_RET file_lread( void far * buf, unsigned long wsize );
int MASTER_RET file_getc(void);
int MASTER_RET file_getw(void);
void MASTER_RET file_skip_until( int data );
unsigned MASTER_RET file_gets( void far * buf, unsigned bsize, int endchar );
long MASTER_RET file_size(void);
unsigned long MASTER_RET file_time(void);
int MASTER_RET file_lsettime( unsigned long filetime );
int MASTER_RET file_settime( unsigned filedate, unsigned filetime );

int MASTER_RET file_create( const char MASTER_PTR * filename );
int MASTER_RET file_append( const char MASTER_PTR * filename );
int MASTER_RET file_write( const void far * buf, unsigned wsize );
int MASTER_RET file_lwrite( const void far * buf, unsigned long wsize );
void MASTER_RET file_putc( int chr );
void MASTER_RET file_putw( int i );

void MASTER_RET file_seek( long pos, int dir );
unsigned long MASTER_RET file_tell(void);
void MASTER_RET file_flush(void);
void MASTER_RET file_close(void);
int MASTER_RET file_exist( const char MASTER_PTR * filename );
int MASTER_RET file_delete( const char MASTER_PTR * filename );

char MASTER_PTR * MASTER_RET file_basename( char MASTER_PTR * pathname );
void MASTER_RET file_splitpath( const char MASTER_PTR *path, char MASTER_PTR *drv, char MASTER_PTR *dir, char MASTER_PTR *name, char MASTER_PTR *ext );
void MASTER_RET file_splitpath_slash( char MASTER_PTR *path, char MASTER_PTR *drv, char MASTER_PTR *dir, char MASTER_PTR *name, char MASTER_PTR *ext );

#define file_assign_buffer(buf,siz)	(file_Buffer = buf, file_BufferSize = siz)
#define file_eof()		file_Eof
#define file_error() 	file_ErrorStat

extern void far * file_Buffer ;
extern unsigned file_BufferSize ;
extern unsigned long file_BufferPos ;

extern unsigned file_BufPtr ;
extern unsigned file_InReadBuf ;
extern int file_Eof ;
extern int file_ErrorStat ;
extern int file_Handle ;

extern int file_sharingmode ;


/* MS-DOS一般 ------------------------------------------------------------ */

void MASTER_RET dos29fake_start(void);
void MASTER_RET dos29fake_end(void);
#if __SC__
int MASTER_RET Dos_getdrive(void);
int MASTER_RET Dos_setdrive( int drive );
#define dos_getdrive() Dos_getdrive()
#define dos_setdrive(d) Dos_setdrive(d)
#else
int MASTER_RET dos_getdrive(void);
int MASTER_RET dos_setdrive( int drive );
#endif
int MASTER_RET dos_chdir( const char MASTER_PTR * path );
int MASTER_RET dos_mkdir( const char MASTER_PTR * path );
int MASTER_RET dos_rmdir( const char MASTER_PTR * path );
int MASTER_RET dos_makedir( const char MASTER_PTR * path );
int MASTER_RET dos_getcwd( int drive, char MASTER_PTR * buf );
long MASTER_RET dos_getdiskfree( int drive );
void MASTER_RET dos_cputs( const char MASTER_PTR * string );
void MASTER_RET dos_cputs2( const char MASTER_PTR * string );
void MASTER_RET dos_putch( int chr );
int MASTER_RET dos_getch(void);
int MASTER_RET dos_getkey(void);
int MASTER_RET dos_getkey2(void);
void MASTER_RET dos_puts( const char MASTER_PTR * str );
void MASTER_RET dos_putp( const char MASTER_PTR * passtr );
void MASTER_RET dos_puts2( const char MASTER_PTR * string );
void MASTER_RET dos_putc( int c );
void MASTER_RET dos_keyclear(void);
unsigned MASTER_RET dos_allocate( unsigned para );
#if __SC__
void MASTER_RET Dos_free( unsigned seg );
#define dos_free(seg) Dos_free(seg)
#else
void MASTER_RET dos_free( unsigned seg );
#endif
unsigned MASTER_RET dos_maxfree(void);
int MASTER_RET dos_ropen( const char MASTER_PTR * filename );
int MASTER_RET dos_create( const char MASTER_PTR * filename, int attrib );
#if __SC__
int MASTER_RET Dos_close( int fh );
#define dos_close(fh) Dos_close(fh)
#else
int MASTER_RET dos_close( int fh );
#endif
int MASTER_RET dos_read( int fh, void far * buffer, unsigned len );
int MASTER_RET dos_write( int fh, const void far * buffer, unsigned len );
long MASTER_RET dos_seek( int fh, long offs, int mode );
long MASTER_RET dos_filesize( int fh );

void MASTER_RET dos_absread( int drive, void far *buf, unsigned pow, long sector );
void MASTER_RET dos_abswrite( int drive, const void far *buf, unsigned pow, long sector );
void MASTER_RET dos_ignore_break(void);
void MASTER_RET dos_accept_break(void);
int MASTER_RET dos_setbreak( int breaksw );
void MASTER_RET dos_get_argv0( char MASTER_PTR * argv0 );

void (interrupt far * MASTER_RET dos_setvect( int vect, void (interrupt far *intfunc)()))();

const char far * MASTER_RET dos_getenv( unsigned envseg, const char MASTER_PTR * envname );

void MASTER_RET dos_setdta( void far * dta );
int MASTER_RET dos_findfirst( const char far * path, int attribute );
int MASTER_RET dos_findnext(void);
struct find_many_t {
	unsigned long time ;
	unsigned long size ;
	char name[13] ;
	char attribute ;
} ;
unsigned MASTER_RET dos_findmany( const char far * path, int attribute, struct find_many_t far * buffer, unsigned max_dir );
int MASTER_RET dos_move( const char far * source, const char far * dest );
long MASTER_RET dos_axdx( int axval, const char MASTER_PTR * strval );
int MASTER_RET dos_copy( int src_fd, int dest_fd, unsigned long copy_len );
#define COPY_ALL	0xffffffffL
int MASTER_RET dos_gets(char MASTER_PTR *buffer, int max);
#if __SC__
int MASTER_RET Dos_get_verify(void);
#define dos_get_verify() Dos_get_verify()
#else
int MASTER_RET dos_get_verify(void);
#endif
void MASTER_RET dos_set_verify_on(void);
void MASTER_RET dos_set_verify_off(void);
int MASTER_RET dos_get_driveinfo(int drive, unsigned MASTER_PTR *cluster, unsigned MASTER_PTR *sector, unsigned MASTER_PTR *bytes);


/* ＶＳＹＮＣ割り込み ----------------------------------------------------- */

extern unsigned volatile vsync_Count1, vsync_Count2 ;
extern unsigned vsync_Delay ;
extern void (far * vsync_Proc)(void);

#define vsync_proc_set(proc)	(CLI(), (vsync_Proc = (void (far *)(void))(proc)), STI())
#define vsync_proc_reset()		(CLI(), (vsync_Proc = 0), STI())

#define vsync_reset1()	(vsync_Count1 = 0)
#define vsync_reset2()	(vsync_Count2 = 0)

#if MASTER98

void MASTER_RET vsync_start(void);
void MASTER_RET vsync_end(void);
void MASTER_RET vsync_enter(void);
void MASTER_RET vsync_leave(void);
void MASTER_RET vsync_wait(void);

extern void (far interrupt * vsync_OldVect)(void);

#endif

#if MASTERV

void MASTER_RET vga_vsync_start(void);
void MASTER_RET vga_vsync_end(void);
void MASTER_RET vga_vsync_wait(void);

extern unsigned vsync_Freq ;

# if !MASTER98
#  define vsync_start() vga_vsync_start(),(vsync_Delay=((vsync_Freq < 21147)\
						? (unsigned)((21147-vsync_Freq)*65536UL/21147):0))
#  define vsync_end()   vga_vsync_end()
#  define vsync_enter() vga_vsync_start()
#  define vsync_leave() vga_vsync_end()
#  define vsync_wait()  vga_vsync_wait()
# endif

#endif

/* タイマ割り込み --------------------------------------------------------- */

extern unsigned long volatile timer_Count ;
extern void (far * timer_Proc)(void);

#if MASTER98

int MASTER_RET timer_start( unsigned count, void (far * timProc)(void));
void MASTER_RET timer_end(void);
void MASTER_RET timer_leave(void);

#endif

#if MASTERV

# if !MASTER98
# endif

#endif

/* 区間計測 --------------------------------------------------------------- */

extern unsigned perform_Rate ;

#if MASTER98
void MASTER_RET perform_start(void);
unsigned long MASTER_RET perform_stop(void);
#endif

#if MASTERV
void MASTER_RET perform_at_start(void);
unsigned long MASTER_RET perform_at_stop(void);

# if !MASTER98
#  define perform_start	perform_at_start
#  define perform_stop	perform_at_stop
# endif
#endif

void MASTER_RET perform_str( char MASTER_PTR * strbuf, unsigned long perf );


/* 文字列操作 ------------------------------------------------------------- */

void MASTER_CRET str_printf( char MASTER_PTR * buf, const char MASTER_PTR * format, ... );

char MASTER_PTR * MASTER_RET str_pastoc( char MASTER_PTR * CString, const char MASTER_PTR * PascalString );
char MASTER_PTR * MASTER_RET str_ctopas( char MASTER_PTR * PascalString, const char MASTER_PTR * CString );

int MASTER_RET str_comma( char MASTER_PTR * buf, long val, unsigned buflen );
int MASTER_RET str_iskanji2( const char MASTER_PTR * str, int n );
unsigned MASTER_RET sjis_to_jis( unsigned sjis );
unsigned MASTER_RET jis_to_sjis( unsigned jis );

/* XMS関係 -----------------------------------------------------------------*/

int MASTER_RET xms_exist(void);
unsigned MASTER_RET xms_allocate( unsigned long memsize );
unsigned MASTER_RET xms_reallocate( unsigned handle, unsigned long newsize );
void MASTER_RET xms_free( unsigned handle );
int MASTER_RET xms_movememory( long destOff, unsigned destHandle, long srcOff, unsigned srcHandle, long Length );
unsigned long MASTER_RET xms_size( unsigned handle );
unsigned long MASTER_RET xms_maxfree(void);
unsigned long MASTER_RET xms_space(void);


/* キー関係 ----------------------------------------------------------------*/

int MASTER_RET key_pressed(void);

#if MASTER98

void MASTER_RET key_start(void);
void MASTER_RET key_end(void);
void MASTER_RET key_set_label( int num, const char MASTER_PTR * lab );
void MASTER_RET key_beep_on(void);
void MASTER_RET key_beep_off(void);
int MASTER_RET gkey_getkey(void);
int MASTER_RET key_shift(void);
int MASTER_RET key_sense( int keygroup );

unsigned MASTER_RET key_scan(void);
unsigned MASTER_RET key_wait(void);

void MASTER_RET key_reset(void);
unsigned MASTER_RET key_wait_bios(void);
unsigned MASTER_RET key_sense_bios(void);

#define K_SHIFT	1
#define K_CAPS	2
#define K_KANA	4
#define K_GRPH	8
#define K_CTRL	16

#endif

extern unsigned key_back_buffer ;

typedef struct KEYTABLE KEYTABLE ;
struct KEYTABLE {
	unsigned rollup, rolldown, ins, del, up, left, right, down ;
	unsigned homeclr, help, s_homeclr ;
} ;
extern KEYTABLE key_table_normal, key_table_alt ;
extern KEYTABLE key_table_shift,  key_table_ctrl ;
#define key_table_grph key_table_alt

	/* key_scanが返す値 ( key_table... ) */
#define CTRL(c)	(c-'@')
#define K_HELP		0x100
#define K_UP		CTRL('E')
#define K_LEFT		CTRL('S')
#define K_RIGHT		CTRL('D')
#define K_DOWN		CTRL('X')
#define K_S_UP		CTRL('R')
#define K_S_LEFT	CTRL('A')
#define K_S_RIGHT	CTRL('F')
#define K_S_DOWN	CTRL('C')
#define K_C_UP		(CTRL('E')+0x200)
#define K_C_LEFT	(CTRL('S')+0x200)
#define K_C_RIGHT	(CTRL('D')+0x200)
#define K_C_DOWN	(CTRL('X')+0x200)
#define K_A_UP		(CTRL('E')+0x300)
#define K_A_LEFT	(CTRL('S')+0x300)
#define K_A_RIGHT	(CTRL('D')+0x300)
#define K_A_DOWN	(CTRL('X')+0x300)
#define K_ROLLUP	CTRL('C')
#define K_ROLLDOWN	CTRL('R')
#define K_S_ROLLUP	CTRL('Z')
#define K_S_ROLLDOWN CTRL('W')
#define K_C_ROLLUP	(CTRL('C')+0x200)
#define K_C_ROLLDOWN (CTRL('R')+0x200)
#define K_A_ROLLUP	(CTRL('C')+0x300)
#define K_A_ROLLDOWN (CTRL('R')+0x300)
#define K_HOMECLR	CTRL('Y')
#define K_CLR		CTRL('@')
#define K_INS		CTRL('V')
#define K_DEL		CTRL('G')
#define K_S_DEL		CTRL('T')
#define K_C_INS		(CTRL('V')+0x200)
#define K_C_DEL		(CTRL('G')+0x200)
#define K_A_INS		(CTRL('V')+0x300)
#define K_A_DEL		(CTRL('G')+0x300)
#define K_BS		CTRL('H')
#define K_TAB		CTRL('I')
#define K_ESC		CTRL('[')
#define K_CR		CTRL('M')
#define K_F1		0x101
#define K_F2		0x102
#define K_F3		0x103
#define K_F4		0x104
#define K_F5		0x105
#define K_F6		0x106
#define K_F7		0x107
#define K_F8		0x108
#define K_F9		0x109
#define K_F10		0x10a
#define K_S_F1		0x10b
#define K_S_F2		0x10c
#define K_S_F3		0x10d
#define K_S_F4		0x10e
#define K_S_F5		0x10f
#define K_S_F6		0x110
#define K_S_F7		0x111
#define K_S_F8		0x112
#define K_S_F9		0x113
#define K_S_F10		0x114
#define K_VF1		0x120
#define K_VF2		0x121
#define K_VF3		0x122
#define K_VF4		0x123
#define K_VF5		0x124
#define K_S_VF1		0x125
#define K_S_VF2		0x126
#define K_S_VF3		0x127
#define K_S_VF4		0x128
#define K_S_VF5		0x129
#define K_C_F1		0x12a
#define K_C_F2		0x12b
#define K_C_F3		0x12c
#define K_C_F4		0x12d
#define K_C_F5		0x12e
#define K_C_F6		0x12f
#define K_C_F7		0x130
#define K_C_F8		0x131
#define K_C_F9		0x132
#define K_C_F10		0x133
#define K_C_VF1		0x134
#define K_C_VF2		0x135
#define K_C_VF3		0x136
#define K_C_VF4		0x137
#define K_C_VF5		0x138

#define key_back(c)	(key_back_buffer = (c))

#if MASTERV

unsigned long MASTER_RET vkey_scan(void);
unsigned long MASTER_RET vkey_wait(void);
unsigned MASTER_RET vkey_to_98( unsigned long atkey );
#define vkey_shift()		(*(unsigned char far *)0x417L)

#define VK_RSHIFT	0x01
#define VK_LSHIFT	0x02
#define VK_SHIFT	(VK_RSHIFT|VK_LSHIFT)
#define VK_CTRL		0x04
#define VK_ALT		0x08
#define VK_KANA		0x10	/* J3100 */
#define VK_NUMLOCK	0x20
#define VK_CAPS		0x40

# if !MASTER98
#  define key_start()					(void)0/* ? */
#  define key_end()						(void)0/* ? */
#  define key_set_label(n,lab)			(void)0/* ? */
#  define key_beep_on()					(void)0
#  define key_beep_off()				(void)0
#  define gkey_getkey()					0/* ? */
#  define key_shift()					vkey_shift()
#  define key_sense(kg)					0/* ? */
#  define key_scan()					vkey_to_98(vkey_scan())
#  define key_wait()					vkey_to_98(vkey_wait())

#  define key_reset()					(void)0
#  define key_wait_bios()				vkey_to_98(vkey_wait())
#  define key_sense_bios()			(key_pressed() ? vkey_to_98(vkey_wait()):0)

#  define K_SHIFT	VK_SHIFT
#  define K_CAPS	VK_CAPS
#  define K_KANA	VK_KANA
#  define K_GRPH	VK_ALT
#  define K_CTRL	VK_CTRL
# endif
#endif


/* 外字関係 ----------------------------------------------------------------*/

#if MASTER98

void MASTER_RET gaiji_putc( unsigned x, unsigned y, unsigned c );
void MASTER_RET gaiji_putca( unsigned x, unsigned y, unsigned c, unsigned atrb );
void MASTER_RET gaiji_puts( unsigned x, unsigned y, const char MASTER_PTR * str );
void MASTER_RET gaiji_putsa( unsigned x, unsigned y, const char MASTER_PTR * str, unsigned atrb );
void MASTER_RET gaiji_putp( unsigned x, unsigned y, const char MASTER_PTR * pstr );
void MASTER_RET gaiji_putpa( unsigned x, unsigned y, const char MASTER_PTR * pstr, unsigned atrb );
void MASTER_RET gaiji_putni( unsigned x, unsigned y, unsigned val, unsigned width );
void MASTER_RET gaiji_putnia( unsigned x, unsigned y, unsigned val, unsigned width, unsigned atrb );

void MASTER_RET gaiji_write( int code, const void far * pattern );
void MASTER_RET gaiji_write_all( const void far * pattern );
void MASTER_RET gaiji_read( int code, void far * pattern );
void MASTER_RET gaiji_read_all( void far * pattern );

int MASTER_RET gaiji_backup(void);
int MASTER_RET gaiji_restore(void);
int MASTER_RET gaiji_entry_bfnt( const char MASTER_PTR * filename );

#endif

#if MASTERV
# if !MASTER98

#  define gaiji_putsa(x,y,s,a)		vga4_bfnt_puts((x)*8,(y)*16,16,(s),\
	vtext_colortable[vtext_color_98(a)&0x0f] + (vga4_readdot((x)*8,(y)*16)<<4))
#  define gaiji_puts(x,y,s)			vga4_bfnt_puts((x)*8,(y)*16,16,(s),\
										0x07)	/* ? */
#  define gaiji_putnia(x,y,v,w,a)	{char ___s[8];str_printf(___s,"%07d",(v));\
									gaiji_putsa(x,y,___s+7-(w),a);}
#  define gaiji_putca(x,y,c,a)		vga4_bfnt_putc((x)*8,(y)*16,(c),\
	vtext_colortable[vtext_color_98(a)&0x0f] + (vga4_readdot((x)*8,(y)*16)<<4))
#  define gaiji_putc(x,y,c)		vga4_bfnt_putc((x)*8,(y)*16,(c),0x07) /* ? */
#  define gaiji_read(c,pat)			_fmemcpy(pat,SEG2FP(font_AnkSeg+(c)*2),32)
#  define gaiji_read_all(pat)		_fmemcpy(pat,SEG2FP(font_AnkSeg),32*256)
#  define gaiji_backup()			1
#  define gaiji_restore()			1
#  define gaiji_entry_bfnt(name)	!font_entry_bfnt(name)	/* ? */
# endif
#endif

/* フォント関係 ------------------------------------------------------------*/

int MASTER_RET font_entry_bfnt( const char MASTER_PTR * );
int MASTER_RET wfont_entry_bfnt( const char MASTER_PTR * );
void MASTER_RET font_write( unsigned code, const void MASTER_PTR * pattern );

#if MASTER98

int MASTER_RET font_entry_cgrom( unsigned firstchar, unsigned lastchar );
void MASTER_RET font_entry_kcg(void);
void MASTER_RET font_entry_gaiji(void);

void MASTER_RET font_read( unsigned code, void MASTER_PTR * pattern );

#endif

#if MASTERV

void MASTER_RET font_at_init(void);
int MASTER_RET font_at_read( unsigned ccode, unsigned fontsize, void far * buf);
int MASTER_RET font_at_entry_cgrom( unsigned firstchar, unsigned lastchar );

# if !MASTER98
#  define font_read(code,buf)		font_at_read(code,0,buf)
#  define font_entry_cgrom(fc,lc)	font_at_entry_cgrom(fc,lc)
#  define font_entry_kcg()			font_at_entry_cgrom(0x20,0xdf)
#  define font_entry_gaiji()		(void)0/* ? */
# endif

#endif

/* マウス関係 --------------------------------------------------------------*/

/* MOUSE2: マウス管理 */
struct mouse_info {
	unsigned button ;
	int x, y ;
} ;
#define mouse_get(ms)	(CLI(),(ms)->x=mouse_X,(ms)->y=mouse_Y,(ms)->button=mouse_Button,STI())

#if MASTER98

/* MOUSE1: マウス割り込み */
void MASTER_RET mouse_int_start( int (far pascal * mousefunc)(void), int freq );
void MASTER_RET mouse_int_end(void);
void MASTER_RET mouse_int_enable(void);
void MASTER_RET mouse_int_disable(void);

#endif
#define MOUSE_120Hz	0
#define MOUSE_60Hz	1
#define MOUSE_30Hz	2
#define MOUSE_15Hz	3

#if MASTERV
# if !MASTER98
#   define mouse_int_start(func,freq)	mousex_start()
#   define mouse_int_end()				mousex_end()
#   define mouse_int_enable()			(void)0
#   define mouse_int_disable()			(void)0
# endif
#endif

#if MASTER98
void MASTER_RET mouse_proc_init(void);
void MASTER_RET mouse_resetrect(void);
void MASTER_RET mouse_setrect( int x1, int y1, int x2, int y2 );
int far pascal mouse_proc(void);
#endif
#if MASTERV
# if !MASTER98
#   define mouse_proc_init()			/* ? */
#   define mouse_resetrect()			/* ? */
#   define mouse_setrect(x1,y1,x2,y2)	mousex_setrect(x1,y1,x2,y2)
#   define mouse_proc()					0/* ? */
# endif
#endif

extern unsigned mouse_Type ;
extern volatile int mouse_X, mouse_Y ;
extern volatile unsigned mouse_Button ;
extern unsigned mouse_ScaleX, mouse_ScaleY, mouse_EventMask ;
extern void (far *mouse_EventRoutine)(void);

#define MOUSE_BRUP		0x80
#define MOUSE_BRDOWN	0x40
#define MOUSE_BLUP		0x20
#define MOUSE_BLDOWN	0x10
#define MOUSE_NOEVENT	0x08
#define MOUSE_MOVE		0x04
#define MOUSE_BR		0x02
#define MOUSE_BL		0x01
#define MOUSE_EVENT		0xf4

/* APIとグラフィックカーソル管理 */

void MASTER_RET mouse_setmickey( unsigned mx, unsigned my );
void MASTER_RET mouse_cmoveto( int x, int y );

#if MASTER98

/* MOUSEI: 簡略マウス割り込み処理 */
void MASTER_RET mouse_istart( int blc, int whc );
void MASTER_RET mouse_iend(void);

/* MOUSEV: 簡略vsync割り込みマウス処理 */
void MASTER_RET mouse_vstart( int blc, int whc );
void MASTER_RET mouse_vend(void);

#endif

/* MOUSEX: 外部マウスドライバ制御 */

#define MOUSEX_NONE	0	/* 外部マウスドライバ不在 */
#define MOUSEX_NEC	1	/* mouse.sys */
#define MOUSEX_MS	2	/* mouse.com */

int MASTER_RET mousex_start(void);
void MASTER_RET mousex_end(void);
void MASTER_RET mousex_moveto( int x, int y );
void MASTER_RET mousex_setrect( int x1, int y1, int x2, int y2 );
void MASTER_RET mousex_istart( int blc, int whc );
void MASTER_RET mousex_iend(void);

#if MASTERV
# if !MASTER98
#  define mouse_istart(b,w)	mousex_istart(b,w)
#  define mouse_iend()		mousex_iend()
#  define mouse_vstart(b,w)	vga_vsync_start(),mousex_istart(b,w)
#  define mouse_vend()		mousex_iend(),vga_vsync_end()
# endif
#endif

/* グラフィックカーソル ----------------------------------------------------*/

void MASTER_RET cursor_init(void);
int MASTER_RET cursor_show(void);
int MASTER_RET cursor_hide(void);
void MASTER_RET cursor_moveto( int x, int y );
void MASTER_RET cursor_pattern( int px, int py, int blc, int whc, const void far * pattern );
void MASTER_RET cursor_pattern2( int px, int py, int whc, const void far * pattern );

struct CursorData {
	unsigned char px, py ;
	unsigned pattern[32] ;
} ;

extern struct CursorData cursor_Arrow ;
extern struct CursorData cursor_Cross ;
extern struct CursorData cursor_Hand ;
extern struct CursorData cursor_Ok ;

#define cursor_setpattern(pat,blc,whc) cursor_pattern(pat.px,pat.py,(blc),(whc),pat.pattern)

/* ビープ関係 --------------------------------------------------------------*/

# define BEEP_FREQ_MAX	65535U

#if MASTER98
void MASTER_RET beep_freq( unsigned freq );

# define SYS_BEEP_FREQ	2000
# define BEEP_FREQ_MIN	38

# define beep_on_98()	OUTB(0x37,6)
# define beep_off_98()	OUTB(0x37,7)
# define beep_on()	beep_on_98()
# define beep_off()	beep_off_98()
# define beep_end()	(beep_off(),beep_freq(SYS_BEEP_FREQ))
#endif

#if MASTERV
# define VBEEP_FREQ_MIN	19

void MASTER_RET vbeep_freq( unsigned freq );
# define beep_on_at()	OUTB(0x61,INPB(0x61) | 3)
# define beep_off_at()	OUTB(0x61,INPB(0x61) & ~3)

# ifndef beep_on
#  define SYS_BEEP_FREQ	2000	/* dummy */
#  define BEEP_FREQ_MIN	VBEEP_FREQ_MIN
#  define beep_on() beep_on_at()
#  define beep_off() beep_off_at()
#  define beep_end() beep_off()
#  define beep_freq(f) vbeep_freq(f)
# endif
#endif

/* DOSメモリ関係 -----------------------------------------------------------*/

#ifndef __TURBOC__
# define _DS	((unsigned)(((unsigned long)(void far *)(void near *)1) >> 16))
#endif
#ifndef MK_FP
# define MK_FP(s,o)	((void far *)(((unsigned long)(s) << 16)+(unsigned)(o)))
#endif
#define _FPSEG(fp)			((unsigned)((unsigned long)(fp) >> 16))
#define _FPOFF(fp)			((unsigned)(unsigned long)(fp))

				/* farポインタを正規化したセグメントを得る */
#define FP_REGULAR_SEG(fp)	(_FPSEG(fp) + (_FPOFF(fp) >> 4))
				/* farポインタを正規化したオフセットを得る */
#define FP_REGULAR_OFF(fp)	(_FPOFF(fp) & 15)
				/* farポインタを正規化する */
#define FP_REGULAR(fp)		(MK_FP(FP_REGULAR_SEG(fp),FP_REGULAR_OFF(fp)))
				/* バイト数の所要パラグラフ数を得る */
#define BYTE2PARA(byteval)	(unsigned)(((((long)(byteval))&0xfffffL)+15)>>4)
				/* セグメント値をポインタに変換する */
#define SEG2FP(seg)			(MK_FP((seg),0))
				/* farポインタから 0:0 番地からのlongオフセット値を得る */
#define FP2LONG(fp)			(((long)_FPSEG(fp) << 4) + _FPOFF(fp))
				/* 0:0番地からのlongオフセット値を farポインタに変換 */
#define LONG2FP(l)			MK_FP((unsigned)((l)>> 4),(unsigned)((l)&0x000f))
				/* farポインタにlong値を加算する */
#define FPADD(fp,l)			LONG2FP(FP2LONG(fp)+(l))


				/* long値の上位16ビットを得る */
#define HIWORD(longval)		(unsigned)((longval) >> 16)
				/* long値の下位16ビットを得る */
#define LOWORD(longval)		(unsigned)(longval)

				/* メモリブロックの用途ID */
#define MEMID_UNKNOWN	0
#define MEMID_FONT		1
#define MEMID_GAIJI		2
#define MEMID_WFONT		3
#define MEMID_SUPER		4
#define MEMID_VVRAM		5
#define MEMID_BFILE		6
#define MEMID_PFILE		7
#define MEMID_BGM		8
#define MEMID_EFS		9
#define MEMID_PI		10
#define MEMID_MAG		11
#define MEMID_TEXTBACK	12
#define MEMID_VTEXTX	13

unsigned MASTER_RET mem_allocate( unsigned para );
unsigned MASTER_RET mem_lallocate( unsigned long bytesize );
void MASTER_RET mem_free( unsigned seg );

void MASTER_RET mem_assign( unsigned top_seg, unsigned parasize );
void MASTER_RET mem_assign_all(void);
int MASTER_RET mem_unassign(void);
int MASTER_RET mem_assign_dos( unsigned parasize );
unsigned MASTER_RET smem_wget( unsigned bytesize );
unsigned MASTER_RET smem_lget( unsigned long bytesize );
void MASTER_RET smem_release( unsigned memseg );
unsigned MASTER_RET hmem_alloc( unsigned parasize );
unsigned MASTER_RET hmem_allocbyte( unsigned bytesize );
unsigned MASTER_RET hmem_lallocate( unsigned long bytesize );
unsigned MASTER_RET hmem_reallocbyte( unsigned oseg, unsigned bytesize );
unsigned MASTER_RET hmem_realloc( unsigned oseg, unsigned parasize );
void MASTER_RET hmem_free( unsigned memseg );
unsigned MASTER_RET hmem_maxfree(void);

extern unsigned mem_TopSeg, mem_OutSeg, mem_EndMark ;
extern unsigned mem_TopHeap, mem_FirstHole ;
extern unsigned mem_MyOwn, mem_AllocID ;

				/* 現在確保できる最大smemパラグラフサイズを得る */
#define smem_maxfree()	(mem_TopHeap - mem_EndMark)
				/* 特定のhmemメモリブロックの現在のパラグラフサイズを得る */
#define hmem_getsize(mseg)	(*(unsigned far *)MK_FP((mseg)-1,2) - (mseg))
				/* hmemメモリブロックの用途IDを得る */
#define hmem_getid(mseg)	(*(unsigned far *)MK_FP((mseg)-1,4))

/* RS-232C関係 -------------------------------------------------------------*/

#ifndef __SIO_H
# if MASTER98

#define SIO_Nxx 0x00	/* Non Parity */
#define SIO_Oxx 0x10	/* Odd Parity */
#define SIO_Exx 0x30	/* Even Parity */
#define SIO_x7x 0x08	/* data length 7bit */
#define SIO_x8x 0x0c	/* data length 8bit */
#define SIO_xx1 0x40	/* stop bit length 1bit */
#define SIO_xx2 0x80	/* stop bit length 1.5bit */
#define SIO_xx3 0xc0	/* stop bit length 2bit */

#define SIO_N81	(SIO_Nxx|SIO_x8x|SIO_xx1)
#define SIO_N82	(SIO_Nxx|SIO_x8x|SIO_xx2)
#define SIO_N83	(SIO_Nxx|SIO_x8x|SIO_xx3)
#define SIO_E81	(SIO_Exx|SIO_x8x|SIO_xx1)
#define SIO_E82	(SIO_Exx|SIO_x8x|SIO_xx2)
#define SIO_E83	(SIO_Exx|SIO_x8x|SIO_xx3)
#define SIO_O81	(SIO_Oxx|SIO_x8x|SIO_xx1)
#define SIO_O82	(SIO_Oxx|SIO_x8x|SIO_xx2)
#define SIO_O83	(SIO_Oxx|SIO_x8x|SIO_xx3)

#define SIO_N71	(SIO_Nxx|SIO_x7x|SIO_xx1)
#define SIO_N72	(SIO_Nxx|SIO_x7x|SIO_xx2)
#define SIO_N73	(SIO_Nxx|SIO_x7x|SIO_xx3)
#define SIO_E71	(SIO_Exx|SIO_x7x|SIO_xx1)
#define SIO_E72	(SIO_Exx|SIO_x7x|SIO_xx2)
#define SIO_E73	(SIO_Exx|SIO_x7x|SIO_xx3)
#define SIO_O71	(SIO_Oxx|SIO_x7x|SIO_xx1)
#define SIO_O72	(SIO_Oxx|SIO_x7x|SIO_xx2)
#define SIO_O73	(SIO_Oxx|SIO_x7x|SIO_xx3)

#define SIO_FLOW_NONE	0
#define SIO_FLOW_HARD	1
#define SIO_FLOW_SOFT	2

#define SIO_MIDI	128
#define SIO_38400	9
#define SIO_20800	8
#define SIO_19200	8
#define SIO_9600	7
#define SIO_4800	6
#define SIO_2400	5
#define SIO_1200	4
#define SIO_600		3
#define SIO_300		2
#define SIO_150		1

#define SIO_ER		0x02	/* sio_bit_on/off() DTR */
#define SIO_ERRCLR	0x10	/* sio_bit_on/off() Errorフラグクリア */
#define SIO_RS		0x20	/* sio_bit_on/off() RTS */
#define SIO_BREAK	0x80	/* sio_bit_on/off() ブレーク信号 */

#define SIO_PERR	0x08	/* sio_read_err() parity error */
#define SIO_OERR	0x10	/* sio_read_err() over run error */
#define SIO_FERR	0x20	/* sio_read_err() framing error */

#define SIO_CI		0x80	/* sio_read_signal() 着呼検出 */
#define SIO_CS		0x40	/* sio_read_signal() 送信可 */
#define SIO_CD		0x20	/* sio_read_signal() キャリア検出 */

#define SIO_SENDBUF_SIZE	2048
#define SIO_RECEIVEBUF_SIZE	6144

#define sio_receivebuf_len(port)	sio_ReceiveLen
#define sio_sendbuf_len(port)		sio_SendLen
#define sio_sendbuf_space(port)		((unsigned)(SIO_SENDBUF_SIZE - sio_SendLen))
#define sio_error_reset(port)		(sio_bit_on((port),SIO_ERRCLR),sio_bit_off((port),SIO_ERRCLR))
#define sio_enter(port,flow)		sio_start(port,0,0,flow)

void MASTER_RET sio_start( int port, int speed, int param, int flow );
void MASTER_RET sio_end( int port );
void MASTER_RET sio_leave( int port );
void MASTER_RET sio_setspeed( int port, int speed );
void MASTER_RET sio_enable( int port );
void MASTER_RET sio_disable( int port );
int MASTER_RET sio_putc( int port, int c );
int MASTER_RET sio_getc( int port );
unsigned MASTER_RET sio_puts( int port, const char MASTER_PTR * sendstr );
unsigned MASTER_RET sio_putp( int port, const char MASTER_PTR * passtr );
unsigned MASTER_RET sio_write( int port, const void MASTER_PTR * senddata, unsigned sendlen );
unsigned MASTER_RET sio_read( int port, void MASTER_PTR * recbuf, unsigned reclen );
void MASTER_RET sio_bit_off( int port, int mask );
void MASTER_RET sio_bit_on( int port, int mask );
int MASTER_RET sio_read_signal( int port );
int MASTER_RET sio_read_err( int port );
int MASTER_RET sio_read_dr( int port );

extern const unsigned sio_SendLen ;
extern const unsigned sio_ReceiveLen ;

# endif		/* MASTER98 */
#endif		/* __SIO_H */

/* BEEP PCM関係 ------------------------------------------------------------*/

void MASTER_RET pcm_convert( void far * dest , const void far * src, unsigned rate, unsigned long size );
#if MASTER98
void MASTER_RET pcm_play( const void far *pcm, unsigned rate, unsigned long size);

# define PCM_22KHz		((INPB(0x42)&0x20) ? (1997/22) : (2458/22))
# define PCM_15_6KHz	((INPB(0x42)&0x20) ? (19968/156) : (24576/156))
# define PCM_22kHz		PCM_22KHz
# define PCM_15_6kHz	PCM_15_6KHz
#endif

/* PC-9801 ジョイスティック入力関係 ----------------------------------------*/

extern int js_bexist, js_shift, js_2player ;
extern unsigned js_stat[2] ;

#define JS_NORMAL		0
#define JS_FORCE_USE	1
#define JS_IGNORE		2
#define JS_UP		0x01
#define JS_DOWN		0x02
#define JS_LEFT		0x04
#define JS_RIGHT	0x08
#define JS_TRIG1	0x10
#define JS_TRIG2	0x20
#define JS_TRIG3	0x40
#define JS_TRIG4	0x80
#define JS_ESC		0x100
#define JS_IRST1	JS_TRIG3
#define JS_IRST2	JS_TRIG4

#define JSA_A		0x80
#define JSA_B		0x40
#define JSA_C		0x20
#define JSA_D		0x10
#define JSA_E1		0x08
#define JSA_E2		0x04
#define JSA_START	0x02
#define JSA_SELECT	0x01

typedef void near pascal JS_ASSIGN_CODE(void);
void MASTER_RET js_key( unsigned func, int group, int maskbit );

#if MASTER98

int MASTER_RET js_start( int force );

void MASTER_RET js_end(void);
int MASTER_RET js_sense(void);
int MASTER_RET js_sense2(void);
int MASTER_RET js_analog( int player, unsigned char MASTER_PTR * astat );

extern unsigned js_saj_port ;

extern JS_ASSIGN_CODE JS_1P4 ;
extern JS_ASSIGN_CODE JS_1P3 ;
extern JS_ASSIGN_CODE JS_1P2 ;
extern JS_ASSIGN_CODE JS_1P1 ;
extern JS_ASSIGN_CODE JS_2P2 ;
extern JS_ASSIGN_CODE JS_2P1 ;
extern JS_ASSIGN_CODE JS_2PRIGHT ;
extern JS_ASSIGN_CODE JS_2PLEFT ;
extern JS_ASSIGN_CODE JS_2PDOWN ;
extern JS_ASSIGN_CODE JS_2PUP ;

#define js_keyassign(func,group,maskbit) js_key((unsigned)func,(group)+0x52a,(maskbit))
#define js_key2player(flag)	(((js_2player=(flag))!=0) ? \
			 (js_keyassign(JS_1P1,9,0x40),js_keyassign(JS_1P2,8,0x80)) \
			:(js_keyassign(JS_1P1,5,0x02),js_keyassign(JS_1P2,5,0x04)))

#endif

#if MASTERV

#define AT_JS_RESIDLEN 12
extern const char AT_JS_RESID[AT_JS_RESIDLEN] ;

struct AT_JS_CALIBDATA {
	char id[AT_JS_RESIDLEN];
	char filler[16-(AT_JS_RESIDLEN&15)];
	Point dmin, dmax, dcenter;
	Point amin, amax, acenter;
} ;
#define AT_JS_RESPARASIZE (((sizeof (struct AT_JS_CALIBDATA))+15)>>4)

extern unsigned at_js_resseg ;

extern unsigned at_js_mintime ;
extern unsigned at_js_maxtime ;

extern unsigned at_js_count ;
extern unsigned at_js_x1,at_js_y1 ;
extern unsigned at_js_x2,at_js_y2 ;
extern unsigned at_js_min ;
extern unsigned at_js_max ;
extern int at_js_fast ;

int MASTER_RET at_js_start( int mode );
void MASTER_RET at_js_end(void);
int MASTER_RET at_js_sense(void);
#define at_js_keyassign(f,g,m) js_key((unsigned)f,(g)+(unsigned)js_map,(m))
#define at_js_key2player(flag)	(((js_2player=(flag))!=0) ? \
	(at_js_keyassign(AT_JS_1P1,10,0x04),at_js_keyassign(AT_JS_1P2,9,0x10)) \
	:(at_js_keyassign(AT_JS_1P1, 5,0x10),at_js_keyassign(AT_JS_1P2,5,0x20)))

int MASTER_RET at_js_wait(Point *p);
void MASTER_RET at_js_calibrate(const Point far * min, const Point far * max, const Point far * center);
#define at_js_resptr ((struct AT_JS_CALIBDATA far *)SEG2FP(at_js_resseg))
#define at_js_get_calibrate()	(at_js_resseg=resdata_exist(AT_JS_RESID,\
									AT_JS_RESIDLEN,AT_JS_RESPARASIZE))

extern JS_ASSIGN_CODE AT_JS_1P4 ;
extern JS_ASSIGN_CODE AT_JS_1P3 ;
extern JS_ASSIGN_CODE AT_JS_1P2 ;
extern JS_ASSIGN_CODE AT_JS_1P1 ;
extern JS_ASSIGN_CODE AT_JS_2P2 ;
extern JS_ASSIGN_CODE AT_JS_2P1 ;
extern JS_ASSIGN_CODE AT_JS_2PRIGHT ;
extern JS_ASSIGN_CODE AT_JS_2PLEFT ;
extern JS_ASSIGN_CODE AT_JS_2PDOWN ;
extern JS_ASSIGN_CODE AT_JS_2PUP ;

extern JS_ASSIGN_CODE js_map ;

# if !MASTER98
#  define js_start(m)	((at_js_fast=1),at_js_start(m),\
		(at_js_get_calibrate()!=0)?(at_js_calibrate(&at_js_resptr->dmin,\
			&at_js_resptr->dmax,&at_js_resptr->dcenter),js_bexist):js_bexist)
#  define js_end()		at_js_end()
#  define js_sense()	at_js_sense()
#  define js_sense2()	(js_stat[0] & JS_ESC)
#  define js_keyassign(f,g,m)	at_js_keyassign(f,g,m)
#  define js_key2player(flag)	at_js_key2player(flag)
#endif

#endif

/* スーパーインポーズ処理 --------------------------------------------------*/
/* 出典: super.lib(c)Kazumi supersfx.lib(c)iR
$Id: super.h 0.36 93/02/19 20:23:11 Kazumi Rel $
*/

#define SUPER_MAXPAT	512

/* extern */
extern unsigned super_patnum ;
extern unsigned super_buffer ;
extern unsigned super_patdata[SUPER_MAXPAT] ;
extern unsigned super_patsize[SUPER_MAXPAT] ;
extern const char BFNT_ID[5] ;	/* "BFNT\x1a" */

/* define */
#define SIZE8x8 0x0108
#define SIZE16x16 0x0210
#define SIZE24x24 0x0318
#define SIZE32x32 0x0420
#define SIZE40x40 0x0528
#define SIZE48x48 0x0630
#define SIZE56x56 0x0738
#define SIZE64x64 0x0840
#define xSIZE8 0x0100
#define xSIZE16 0x0200
#define xSIZE24 0x0300
#define xSIZE32 0x0400
#define xSIZE40 0x0500
#define xSIZE48 0x0600
#define xSIZE56 0x0700
#define xSIZE64 0x0800
#define ySIZE8 0x0008
#define ySIZE16 0x0010
#define ySIZE24 0x0018
#define ySIZE32 0x0020
#define ySIZE40 0x0028
#define ySIZE48 0x0030
#define ySIZE56 0x0038
#define ySIZE64 0x0040
#define PATTERN_ERASE 0
#define PATTERN_BLUE 1
#define PATTERN_RED 2
#define PATTERN_GREEN 3
#define PATTERN_INTEN 4
#define PLANE_ERASE 0x00c0
#define PLANE_BLUE 0xffce
#define PLANE_RED 0xffcd
#define PLANE_GREEN 0xffcb
#define PLANE_INTEN 0xffc7

			/* super.lib エラーコード */
#ifndef CommonDefined
#define NoError 0						/* 正常終了 */
#define InvalidFunctionCode -1			/* 無効なファンクションコード */
#define FileNotFound -2					/* ファイル名が見つからない */
#define PathNotFound -3					/* パス名が見つからない */
#define TooManyOpenFiles -4				/* オープンファイル過多 */
#define AccessDenied -5					/* アクセスできない */
#define InvalidHandle -6				/* 無効なハンドル */
#define MemoryControlBlocksDestroyed -7	/* メモリコントロールブロック破損 */
#define InsufficientMemory -8			/* メモリ不足 */
#define InvalidMemoryBlockAddress -9	/* 無効なメモリブロックアドレス */
#define InvalidEnvironment -10			/* 無効な環境 */
#define InvalidFormat -11				/* 無効な書式 */
#define InvalidAccessCode -12			/* 無効なアクセスコード */
#define InvalidData -13					/* 無効なデータ */
#define InvalidDrive -15				/* 無効なドライブ名 */
#define AttemptToRemoveCurrentDirectory -16
							/* カレントディレクトリを削除しようとした */
#define NotSameDevice -17				/* 同じデバイスではない */
#define NoMoreFiles -18					/* これ以上ファイルはない */
#define DiskIsWriteProtected -19		/* ディスクがライトプロテクト状態 */
#define BadDiskUnit -20					/* ディスクユニット不良 */
#define DriveNotReady -21				/* ドライブが準備されていない */
#define InvalidDiskCommand -22			/* 無効なディスクコマンド */
#define CrcError -23					/* CRC エラー */
#define InvalidLength -24				/* 無効な長さ */
#define SeekError -25					/* シークエラー */
#define NotAnMsdosDisk -26				/* MS-DOS のディスクではない */
#define SectorNotFound -27				/* セクタが見つからない */
#define OutOfPaper -28					/* 紙切れ */
#define WriteFault -29					/* 書き込み失敗 */
#define ReadFault -30					/* 読み出し失敗 */
#define GeneralFailure -31				/* 通常の失敗 */
#define ShareingViolation -32			/* シェアリング違反 */
#define LockViolation -33				/* ロック違反 */
#define WrongDisk -34					/* ディスク指定の失敗 */
#define FcbUnavailable -35				/* FCB 使用不可能 */
#define NetworkRequestNotSupported -50
							/* ネットワークリクエストが準備されていない */
#define RemoteComputerNotListening -51
							/* リモートコンピュータが LISTEN していない */
#define DuplicateNameOnNetwork -52		/* ネットワーク名の 2 重定義 */
#define NetworkNameNotFound -53			/* ネットワーク名が見つからない */
#define NetworkBusy -54					/* ネットワークビジー */
#define NetworkDeviceNoLongerExists -55
							/* ネットワークデバイスはこれ以上ない */
#define NetBiosCommandLimitExceeded -56	/* ネットワーク BIOS の限界を越えた */
#define NetworkAdapterHardwareError -57
							/* ネットワークアダプタのハードエラー */
#define IncorrectResponseFromNetwork -58
							/* ネットワークからの不当な応答 */
#define UnexpectedNetworkError -59		/* 予期できないネットワークエラー */
#define IncompatibleRemoteAdapter -60	/* リモートアダプタが合致しない */
#define PrintQueueFull -61				/* プリント待ち行列が一杯 */
#define QueueNotFull -62				/* 待ち行列は一杯ではない */
#define NotEnoughSpaceForPrintFile -63
							/* プリントファイルのためのスペースが不十分 */
#define NetworkNameWasDeleted -64
							/* ネットワーク名は既に削除されている */
#define AccessDenied2 -65				/* アクセスできない */
#define NetworkDeviceTypeIncorrect -66
							/* ネットワークデバイスのタイプが不当 */
#define NetworkNameNotFound2 -67		/* ネットワーク名が見つからない */
#define NetworkNameLimitExceeded -68	/* ネットワーク名の限界を越えた */
#define NetBiosSessionLimitExceeded -69
							/* ネットワーク BIOS セッションの限界を越えた */
#define TemporarilyPaused -70			/* 一時休止 */
#define NetworkRequestNotAccepted -71
							/* ネットワークの要求が受けつけられない */
#define PrintOrDiskRedirectionIsPaused -72
							/* プリンタ,ディスクのリディレクション休止 */
#define FileExists -80					/* ファイルが存在する */
#define CannotMake -82					/* 作成不能 */
#define Interrupt24hFailure -83			/* 割り込みタイプ 24H の失敗 */
#define OutOfStructures -84				/* ストラクチャの不良 */
#define AlreadyAssigned -85				/* 割り当て済み */
#define InvalidPassword -86				/* 無効なパスワード */
#define InvalidParameter -87			/* 無効なパラメータ */
#define NetWriteFault -88				/* ネットワークへの書き込み失敗 */
#define CommonDefined
#endif

/* define macro */
#define super_check_entry(patnum) (super_patsize[patnum])
#define super_getsize_pat(patnum) (super_patsize[patnum])
#define super_getsize_pat_x(patnum) ((super_patsize[patnum] >> 8) * 8)
#define super_getsize_pat_y(patnum) (super_patsize[patnum] & 0xff)

/* replacements for lower compatibilty... (super.lib) */
#define peekb2(seg,off)			(*(const unsigned char far *)MK_FP(seg,off))
#define poke2(seg,off,data,len)	_fmemset(MK_FP(seg,off),(data),(len))
#define pokeb2(seg,off,data)	((*(unsigned char far *)MK_FP(seg,off)) = (data))
#ifndef __TURBOC__
# define poke(seg,off,data)	((*(unsigned far *)MK_FP(seg,off)) = (data))
#endif
#define get_ds()		_DS
#define super_palette	Palettes
#define BFNT_HEADER		BfntHeader

/* struct */
typedef struct BfntHeader BfntHeader ;
typedef BfntHeader MASTER_PTR * PBfntHeader ;
struct BfntHeader {
	unsigned char id[5], col ;
	unsigned char ver, x00 ;
	unsigned int Xdots;
	unsigned int Ydots;
	unsigned int START;
	unsigned int END;
	unsigned char font_name[8];
	unsigned long time;
	unsigned int extSize;
	unsigned int hdrSize;
} ;

/* function prototypes */

int MASTER_RET bfnt_header_load(const char MASTER_PTR *filename, BfntHeader MASTER_PTR *header);

int MASTER_RET bfnt_change_erase_pat(int, int, PBfntHeader);
int MASTER_RET bfnt_entry_pat(int, PBfntHeader, int);
int MASTER_RET bfnt_extend_header_analysis(int, PBfntHeader);
int MASTER_RET bfnt_extend_header_skip(int, PBfntHeader);
int MASTER_RET bfnt_header_read(int, PBfntHeader);
int MASTER_RET bfnt_palette_skip(int, PBfntHeader);
int MASTER_RET bfnt_palette_set(int, PBfntHeader);

int MASTER_RET fontfile_open( const char MASTER_PTR * );
int MASTER_RET fontfile_close( int filehandle );

#if MASTER98
void MASTER_RET over_put_8(int x, int y, int num);
void MASTER_RET over_roll_put_8(int x, int y, int num);
void MASTER_RET over_small_put_8(int x, int y, int num);
#endif

int MASTER_RET palette_entry_rgb(const char MASTER_PTR *);

#if MASTER98
void MASTER_RET palette_black_in(unsigned speed);
void MASTER_RET palette_black_out(unsigned speed);
void MASTER_RET palette_white_in(unsigned speed);
void MASTER_RET palette_white_out(unsigned speed);
#endif
#if MASTERV
void MASTER_RET dac_black_in(unsigned speed);
void MASTER_RET dac_black_out(unsigned speed);
void MASTER_RET dac_white_in(unsigned speed);
void MASTER_RET dac_white_out(unsigned speed);
# if !MASTER98
#  define palette_black_in(s)	dac_black_in(s)
#  define palette_black_out(s)	dac_black_out(s)
#  define palette_white_in(s)	dac_white_in(s)
#  define palette_white_out(s)	dac_white_out(s)
# endif
#endif


int MASTER_RET super_cancel_pat(int num);
void MASTER_RET super_clean( int min_pat, int max_pat );
int MASTER_RET super_change_erase_bfnt(int, const char MASTER_PTR *);
void MASTER_RET super_change_erase_pat(int, const void far *);
int MASTER_RET super_entry_bfnt(const char MASTER_PTR *);
int MASTER_RET super_entry_char(int patnum);
int MASTER_RET super_entry_at( int num, int patsize, unsigned pat_seg );
int MASTER_RET super_entry_pat(int patsize, const void far *image, int clear_color);
int MASTER_RET super_entry_pack( const void far * image, unsigned image_width, int patsize, int clear_color );
void MASTER_RET super_free(void);
int MASTER_RET super_backup_ems( unsigned MASTER_PTR * handle, int first_pat, int last_pat );
int MASTER_RET super_restore_ems( unsigned handle, int load_to );
void MASTER_RET super_free_ems(void);
int MASTER_RET super_duplicate(int topat, int frompat);
void MASTER_RET super_hrev(int patnum);
#define super_dup(pat)	super_duplicate(super_patnum,(pat))


#if MASTER98
int MASTER_RET super_convert_tiny( int num );
void MASTER_RET repair_back(int x, int y, int num);
void MASTER_RET repair_out(int x, int y, int num);

void MASTER_RET slice_put(int x, int y, int num, int line);

void MASTER_RET super_in(int x, int y, int num);
void MASTER_RET super_large_put(int x, int y, int num);
void MASTER_RET super_out(int x, int y, int num);
void MASTER_RET super_put(int x, int y, int num);
void MASTER_RET super_put_vrev(int x, int y, int num);
void MASTER_RET super_put_1plane(int x, int y, int num, int pattern_plane, unsigned put_plane );
void MASTER_RET super_put_1plane_8(int x, int y, int num, int pattern_plane, unsigned put_plane );
void MASTER_RET super_put_8(int x, int y, int num);
void MASTER_RET super_put_clip(int x, int y, int num);
void MASTER_RET super_put_clip_8(int x, int y, int num);
#endif
#if MASTERV
# define vga4_repair_back(x,y,num)	(graph_accesspage(0),vga4_byte_move(\
				(x)/8,(y)+graph_VramLines, (x)/8+(super_patsize[num]>>8),\
				(y)+graph_VramLines+(super_patsize[num]&0xff)-1,(x)/8,(y)))
void MASTER_RET vga4_repair_out(int x, int y, int num);
void MASTER_RET vga4_slice_put(int x, int y, int num,int line);
void MASTER_RET vga4_super_in(int x, int y, int num);
void MASTER_RET vga4_super_large_put(int x, int y, int num);
void MASTER_RET vga4_super_put(int x, int y, int num);
void MASTER_RET vga4_super_put_vrev(int x, int y, int num);
void MASTER_RET vga4_super_put_1plane(int x, int y, int num, int pattern_plane, unsigned put_plane );
void MASTER_RET vga4_super_put_1plane_8(int x, int y, int num, int pattern_plane, unsigned put_plane );
void MASTER_RET vga4_super_put_8(int x, int y, int num);
void MASTER_RET vga4_super_put_clip(int x, int y, int num);
void MASTER_RET vga4_over_put_8(int x, int y, int num);
void MASTER_RET vga4_over_roll_put_8(int x, int y, int num);
void MASTER_RET vga4_super_put_rect(int x, int y, int num);
void MASTER_RET vga4_super_put_vrev_rect(int x, int y, int num);
void MASTER_RET vga4_super_roll_put(int x, int y, int num);
# if !MASTER98
#  define super_convert_tiny(n)		0	/* DOS/Vではtinyは無効 */
#  define repair_back(x,y,num)		vga4_repair_back(x,y,num)
#  define repair_out(x,y,num)		vga4_repair_out(x,y,num)
#  define slice_put(x,y,num,l)		vga4_slice_put(x,y,num,l)
#  define super_in(x,y,num)			vga4_super_in(x,y,num)
#  define super_large_put(x,y,num)	vga4_super_large_put(x,y,num)
#  define super_out(x,y,num)		vga4_repair_out(x,y,num)	/* ? */
#  define super_put(x,y,num)		vga4_super_put(x,y,num)
#  define super_put_vrev(x,y,num)	vga4_super_put_vrev(x,y,num)
#  define super_put_1plane(x,y,num,pat,put)	vga4_super_put_1plane(x,y,num,pat,put)
#  define super_put_1plane_8(x,y,num,pat,put)	vga4_super_put_1plane_8(x,y,num,pat,put)
#  define super_put_8(x,y,num)			vga4_super_put_8(x,y,num)
#  define super_put_clip(x,y,num)		vga4_super_put_clip(x,y,num)

#  define over_put_8(x,y,num)			vga4_over_put_8(x,y,num)
#  define over_roll_put_8(x,y,num)		vga4_over_roll_put_8(x,y,num)
#  define over_small_put_8(x,y,num)		/* ? */

#  define super_put_rect(x,y,num)		vga4_super_put_rect(x,y,num)
#  define super_put_vrev_rect(x,y,num)	vga4_super_put_vrev_rect(x,y,num)
#  define super_roll_put(x,y,num)		vga4_super_roll_put(x,y,num)
# endif
#endif

void MASTER_RET super_set_window(int y1, int y2);
#if MASTER98
void MASTER_RET super_put_window(int x, int y, int num);
void MASTER_RET super_put_rect(int x, int y, int num);
void MASTER_RET super_put_vrev_rect(int x, int y, int num);
void MASTER_RET super_repair(int x, int y, int num);
void MASTER_RET super_roll_put(int x, int y, int num);
void MASTER_RET super_roll_put_1plane(int x, int y, int num, int pattern_plane, unsigned put_plane);
void MASTER_RET super_roll_put_1plane_8(int x, int y, int num, int pattern_plane, unsigned put_plane);
void MASTER_RET super_roll_put_8(int x, int y, int num);
void MASTER_RET super_zoom(int x, int y, int num, int zoom);
void MASTER_RET super_zoom_put(int x, int y, int num, unsigned x_rate, int y_rate);
void MASTER_RET super_zoom_put_1plane(int x, int y, int num, unsigned x_rate, unsigned y_rate, int pattern_plane, unsigned put_plane);
#endif

#if MASTER98
void MASTER_RET super_put_tiny( int x, int y, int num );
void MASTER_RET super_put_tiny_small( int x, int y, int num );
void MASTER_RET super_roll_put_tiny( int x, int y, int num );
void MASTER_RET super_put_tiny_vrev( int x, int y, int num );
void MASTER_RET super_put_tiny_small_vrev( int x, int y, int num );
#endif
#if MASTERV
void MASTER_RET vga4_super_zoom(int x, int y, int num, int zoom);
void MASTER_RET vga4_super_zoom_put(int x, int y, int num, unsigned x_rate, int y_rate);
# if !MASTER98	/* DOS/Vでは super_*_tinyは無効 */
#  define super_zoom(x,y,n,z)			vga4_super_zoom(x,y,n,z)
#  define super_zoom_put(x,y,num,xr,yr)	vga4_super_zoom_put(x,y,num,xr,yr) 
#  define super_put_tiny(x,y,n)			super_put(x,y,n)
#  define super_put_tiny_small(x,y,n)	super_put(x,y,n)
#  define super_roll_put_tiny(x,y,n)	super_roll_put(x,y,n)
#  define super_put_tiny_vrev(x,y,n)	super_put_vrev(x,y,n)
#  define super_put_tiny_small_vrev(x,y,n)	super_put_vrev(x,y,n)
# endif
#endif

extern unsigned virtual_seg;
#if MASTER98
int MASTER_RET virtual_copy(void);
void MASTER_RET virtual_repair(int x, int y, int num);
void MASTER_RET virtual_vram_copy(void);
#endif
#if MASTERV
int MASTER_RET vga4_virtual_copy(void);
void MASTER_RET vga4_virtual_repair(int x, int y, int num);
void MASTER_RET vga4_virtual_vram_copy(void);
# if !MASTER98
#  define virtual_copy()			vga4_virtual_copy()
#  define virtual_repair(x,y,num)	vga4_virtual_repair(x,y,num)
#  define virtual_vram_copy()		vga4_virtual_vram_copy()
# endif
#endif
void MASTER_RET virtual_over_put_8(int x, int y, int num);
void MASTER_RET virtual_free(void);


#if MASTER98
void MASTER_RET super_wave_put(int x, int y, int num, int len, char amp, int ph);
void MASTER_RET super_wave_put_1plane(int x, int y, int num, int len, char amp, int ph, int pattern_plane, unsigned put_plane);
void MASTER_RET super_vibra_put(int x, int y, int num, int len, int ph);
void MASTER_RET super_vibra_put_1plane(int x, int y, int num, int len, int ph, int pattern_plane, unsigned put_plane); 
void MASTER_RET super_zoom_v_put(int x, int y, int num, unsigned y_rate);
void MASTER_RET super_zoom_v_put_1plane(int x, int y, int num, unsigned rate, int pattern_plane, unsigned put_plane);

void MASTER_RET over_dot_8(int x, int y);
void MASTER_RET over_blk16_8(int x, int y);

#endif

#if MASTERV

void MASTER_RET vga4_super_wave_put(int x, int y, int num, int len, char amp, int ph);
void MASTER_RET vga4_super_vibra_put(int x, int y, int num, int len, int ph);
void MASTER_RET vga4_super_wave_put_1plane(int x, int y, int num, int len, char amp, int ph, int pattern_plane, unsigned put_plane);

void MASTER_RET vga4_super_zoom_v_put_1plane(int x, int y, int num, unsigned rate, int pattern_plane, unsigned put_plane);

#if !MASTER98
# define super_wave_put(x,y,num,len,amp,ph)	vga4_super_wave_put(x,y,num,len,amp,ph)
# define super_wave_put_1plane(x,y,num,len,amp,ph,pat,put) vga4_super_wave_put_1plane(x,y,num,len,amp,ph,pat,put)
# define super_vibra_put(x,y,num,len,ph)	vga4_super_vibra_put(x,y,num,len,ph)	
# define super_vibra_put_1plane(x,y,num,len,ph,pat,put) super_put_1plane(x,y,num,pat,put)
# define super_zoom_v_put(x,y,num,yrate)	super_zoom_put(x,y,num,256,yrate)
# define super_zoom_v_put_1plane(x,y,num,rate,pat,put) vga4_super_zoom_v_put_1plane(x,y,num,rate,pat,put)

# define over_dot_8(x,y)		vgc_setcolor(VGA_PSET,0),\
		(*(char far *)MK_FP(graph_VramSeg,(y)*graph_VramWidth+(x)/8)='\xff')
# define over_blk16_8(x,y)	vgc_setcolor(VGA_PSET,0),vgc_byteboxfill_x_pset((x),y,(x)+1,(y)+15)
#endif
#endif

/* EGCによるスクロール -----------------------------------------------------*/
/* 出典:
egc.h 0.06
*/

#if MASTER98
void MASTER_RET egc_shift_down(int x1, int y1, int x2, int y2, int dots);
void MASTER_RET egc_shift_down_all(int dots);
void MASTER_RET egc_scroll_left(int dots);
void MASTER_RET egc_shift_left(int x1, int y1, int x2, int y2, int dots);
void MASTER_RET egc_shift_left_all(int dots);
void MASTER_RET egc_scroll_right(int dots);
void MASTER_RET egc_shift_right(int x1, int y1, int x2, int y2, int dots);
void MASTER_RET egc_shift_right_all(int dots);
void MASTER_RET egc_shift_up(int x1, int y1, int x2, int y2, int dots);
void MASTER_RET egc_shift_up_all(int dots);
#endif

#if MASTERV
# if !MASTER98
#  define egc_shift_down_all(dots)	vga4_byte_move(0,0,graph_VramWidth-1,graph_VramLines-(dots),0,(dots))
#  define egc_shift_up_all(dots)	vga4_byte_move(0,(dots),graph_VramWidth-1,graph_VramLines-(dots),0,0)
# endif
#endif

/* グラフィック画面への多角形描画処理 --------------------------------------*/
/* 出典:
gc_poly.h 0.16
*/

/* grcg_setcolor()や vgc_setcolor()に指定するアクセスプレーン指定 */
#define GC_B	0x0e	/* 青プレーンをアクセスする */
#define GC_R	0x0d
#define GC_BR	0x0c	/*	:  */
#define GC_G	0x0b	/*	:  */
#define GC_BG	0x0a	/*	:  */
#define GC_RG	0x09
#define GC_BRG	0x08	/*	:  */
#define GC_I	0x07
#define GC_BI	0x06
#define GC_RI	0x05
#define GC_BRI	0x04
#define GC_GI	0x03
#define GC_BGI	0x02
#define GC_RGI	0x01	/*	:  */
#define GC_BRGI	0x00	/* 全プレーンをアクセスする */

#if MASTER98
/* grcg_setcolor()の modeに設定する値 */
#ifndef GC_OFF
# define GC_OFF	0
# define GC_TDW	0x80	/* 書き込みﾃﾞｰﾀは無視して、ﾀｲﾙﾚｼﾞｽﾀの内容を書く */
# define GC_TCR	0x80	/* ﾀｲﾙﾚｼﾞｽﾀと同じ色のﾋﾞｯﾄが立って読み込まれる */
# define GC_RMW	0xc0	/* 書き込みﾋﾞｯﾄが立っているﾄﾞｯﾄにﾀｲﾙﾚｼﾞｽﾀから書く */
#endif

#define EGC_ACTIVEPLANEREG	0x04a0
#define EGC_READPLANEREG	0x04a2
#define EGC_MODE_ROP_REG	0x04a4
#define EGC_FGCOLORREG		0x04a6
#define EGC_MASKREG			0x04a8
#define EGC_BGCOLORREG		0x04aa
#define EGC_ADDRRESSREG		0x04ac
#define EGC_BITLENGTHREG	0x04ae

#define EGC_COMPAREREAD	0x2000
#define EGC_WS_PATREG	0x1000	/* WS = write source */
#define EGC_WS_ROP		0x0800	/* parren reg, ans of rop, cpu data */
#define EGC_WS_CPU		0x0000
#define EGC_SHIFT_CPU	0x0400	/* input to shifter */
#define EGC_SHIFT_VRAM	0x0000	/* cpu write, vram read */
#define EGC_RL_MEMWRITE	0x0200	/* RL = pattern Register Load */
#define EGC_RL_MEMREAD	0x0100	/* ^at mem write, <-at mem read */
#define EGC_RL_NONE		0x0000	/* no touch */

#define GDC_PSET		0x000
#define GDC_XOR			0x100
#define GDC_AND			0x200
#define GDC_OR			0x300

#define GRAM_400	0xa800
#define GRAM_200A	0xa800
#define GRAM_200B	0xabe8
#endif


typedef struct Point3D Point3D ;
struct Point3D {
	int x, y, z ;
} ;

typedef struct Rotate3D Rotate3D ;
struct Rotate3D {
	int rx, ry, rz ;
} ;

extern const int ClipXL,ClipXW,ClipXR, ClipYT,ClipYH,ClipYB ;
extern const int ClipZH,ClipZD,ClipZY ;
extern unsigned ClipYT_seg, ClipYB_adr ;
extern unsigned near trapez_a[], near trapez_b[] ;
extern const long SolidTile[16] ;

extern unsigned GDC_LineStyle ;
#if MASTER98
extern int GDCUsed ;
extern int GDC_Color, GDC_AccessMask ;
#endif

#if MASTER98
# ifdef grcg_off
#  undef grcg_off	/* 関数定義をするので、変換されるとまずい */
# endif
#endif

/* 設定 */
int MASTER_RET grc_setclip( int xl, int yt, int xr, int yb );
#define grc_setGRamStart(s) (graph_VramSeg = (s))

void MASTER_RET c_make_linework( void near * trapez, int xt, int xb, int ylen );
/* 多角形 */
int MASTER_RET grc_clip_polygon_n( Point MASTER_PTR * dest, int ndest,
								const Point MASTER_PTR * src, int nsrc );
/* 直線 */
int MASTER_RET grc_clip_line( Point MASTER_PTR * p1, Point MASTER_PTR * p2 );

#if MASTER98
void MASTER_RET c_draw_trapezoid( unsigned y, unsigned y2 );
void MASTER_RET c_draw_trapezoidx( unsigned y, unsigned y2 );

void MASTER_RET grcg_setcolor( int mode, int color );
void MASTER_RET grcg_settile_1line( int mode, long tile );
void MASTER_RET grcg_and( int mode, int color );
void MASTER_RET grcg_or( int mode, int color );
void MASTER_RET grcg_off(void);
# define	grcg_setmode(mode)		OUTB(0x7c,mode)
# define	grcg_off()				OUTB(0x7c,0)

/* EGC */
void MASTER_RET egc_on(void);
void MASTER_RET egc_off(void);
void MASTER_RET egc_start(void);
void MASTER_RET egc_end(void);

#define egc_setfgcolor(color)	OUTW(EGC_FGCOLORREG,color)
#define egc_setbgcolor(color)	OUTW(EGC_BGCOLORREG,color)
#define egc_selectfg()			OUTW(EGC_READPLANEREG,0x40ff)
#define egc_selectbg()			OUTW(EGC_READPLANEREG,0x20ff)
#define egc_selectpat()			OUTW(EGC_READPLANEREG,0x00ff)
#define egc_setrop(mode_rop)	OUTW(EGC_MODE_ROP_REG,mode_rop)
#define has_egc() (*((char far *)0x0000054dL) & 0x40)

/* GDC */
void MASTER_RET gdc_wait(void);
void MASTER_RET gdc_waitempty(void);
void MASTER_RET gdc_line( int x1,int y1,int x2,int y2 );
void MASTER_RET gdc_circle( int x, int y, unsigned r );
#define gdc_setlinestyle(style) (GDC_LineStyle = (style))
#define gdc_setcolor(color) (GDC_Color = (color))
#define gdc_setaccessplane(plane) (GDC_AccessMask = (plane))

/* 台形 */
void MASTER_RET grcg_trapezoid (	int y1, int x11, int x12,
								int y2, int x21, int x22 );

/* 多角形 */
void MASTER_RET grcg_polygon_c( const Point MASTER_PTR * pts, int npoint );
void MASTER_RET grcg_polygon_cx( const Point MASTER_PTR * pts, int npoint );
void grcg_polygon_vcx( int npoint, int x1, int y1, ... );

/* 三角形 */
void MASTER_RET grcg_triangle( int x1,int y1, int x2,int y2, int x3,int y3 );
void MASTER_RET grcg_triangle_x( const Point MASTER_PTR * pts );

/* 直線 */
void MASTER_RET grcg_hline( int x1, int x2, int y );
void MASTER_RET grcg_vline( int x, int y1, int y2 );
void MASTER_RET grcg_line( int x1, int y1, int x2, int y2 );
void MASTER_RET grcg_thick_line( int x1, int y1, int x2, int y2,
								unsigned wid, unsigned hei );

/* 四角形塗りつぶし */
void MASTER_RET grcg_boxfill( int x1, int y1, int x2, int y2 );
void MASTER_RET grcg_pset( int x, int y );
void MASTER_RET grcg_fill(void);
void MASTER_RET grcg_round_boxfill( int x1, int y1, int x2, int y2, unsigned r );

/* 円描画 */
void MASTER_RET grcg_circle( int x, int y, unsigned r );
void MASTER_RET grcg_circle_x( int x, int y, unsigned r );
void MASTER_RET grcg_circlefill( int x, int y, unsigned r );

/* 8dot単位の箱塗り */
void MASTER_RET grcg_byteboxfill_x( int x1, int y1, int x2, int y2 );
void MASTER_RET grcg_bytemesh_x( int x1, int y1, int x2, int y2 );

#endif


/* VGA16色グラフィック処理 -------------------------------------------------*/
/* "vgapoly"
 * (c)恋塚, あら
 */

#if MASTERV

#define VGA_PSET	0x00f0
#define VGA_AND		0x08f0
#define VGA_OR		0x10f0
#define VGA_XOR		0x18f0

void MASTER_RET vgc_setcolor(int mask,int color);
void MASTER_RET vgc_hline(int x1,int x2,int y);
void MASTER_RET vgc_vline(int x,int y1,int y2);
void MASTER_RET vgc_line(int x1,int y1,int x2,int y2);
void MASTER_RET vgc_line2(int x1,int y1,int x2,int y2,unsigned lstyle);
void MASTER_RET vgc_thick_line( int x1, int y1, int x2, int y2, unsigned wid, unsigned hei );
void MASTER_RET vgc_boxfill(int x1,int y1,int x2,int y2);
void MASTER_RET vgc_pset(int x,int y);
void MASTER_RET vgc_round_boxfill( int x1, int y1, int x2, int y2, unsigned r );
void MASTER_RET vgc_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 );
void MASTER_RET vgc_polygon_c( const Point MASTER_PTR * pts, int npoint );
void MASTER_RET vgc_polygon_cx( const Point MASTER_PTR * pts, int npoint );
void vgc_polygon_vcx( int npoint, int x1, int y1, ... );
void MASTER_RET vgc_triangle( int x1,int y1, int x2,int y2, int x3,int y3 );
void MASTER_RET vgc_circle_x( int x, int y, int r );
void MASTER_RET vgc_circle( int x, int y, int r );
void MASTER_RET vgc_circlefill( int x, int y, int r );
void MASTER_RET vgc_byteboxfill_x(int x1,int y1,int x2,int y2);
void MASTER_RET vgc_byteboxfill_x_pset(int x1,int y1,int x2,int y2);
void MASTER_RET vgc_bytemesh_x(int x1,int y1,int x2,int y2);

void MASTER_RET vgc_font_puts(int x, int y, int step, const char MASTER_PTR * str );
void MASTER_RET vgc_font_put(int x, int y, const char MASTER_PTR * str );

void MASTER_RET vgc_bfnt_puts(int x, int y, int step, const char MASTER_PTR * str );
void MASTER_RET vgc_bfnt_putc(int x, int y, int ank );
void MASTER_RET vgc_kanji_puts(int x, int y, int step, const char MASTER_PTR * kanji );
void MASTER_RET vgc_kanji_putc(int x, int y, unsigned kanji );

#define VGA_PORT	0x03ce
#define VGA_SET_RESET_REG	0	/* setcolor */
#define VGA_ENABLE_SR_REG	1
#define VGA_COLOR_CMP_REG	2
#define VGA_DATA_ROT_REG	3	/* setcolor */
#define VGA_READPLANE_REG	4	/* ←readplaneは下位2bitにプレーン番号を書く */
#define VGA_MODE_REG		5	/* setcolorでVGA_CHARに設定される */
#define VGA_MULTI_REG		6	/* setcolorでVGA_CHARに設定される */
#define VGA_DISABLECMP		7	/* setcolorでVGA_CHARに設定される */
#define VGA_BITMASK_REG		8	/* setcolorで0ffhに設定される */

	/* VGA_MODE_REGレジスタに設定する内容 */

#define VGA_READPLANE	0	/* readmap選択regの場所をそのまま読む */
#define VGA_COMPARE		8	/* 読み込み時色比較レジスタと一致したらbit on */

#define VGA_NORMAL	0		/* GCのモード */
#define VGA_LATCH	1
#define VGA_FILL	2
#define VGA_CHAR	3

#if !MASTER98

#define GC_RMW	0
#define GC_OFF	0

#define grcg_or(m,col)				vgc_setcolor((m) | VGA_OR,(col))
#define grcg_and(m,col)				vgc_setcolor((m) | VGA_AND,(col))
#define grcg_settile_1line(m,l)		/* むむっ */
#define grcg_setcolor(m,col)		vgc_setcolor((m) | VGA_PSET,(col))
#define grcg_off()					(void)0/* 消す */

#define grcg_trapezoid(y1,x11,x12,y2,x21,x22)	vgc_trapezoid(y1,x11,x12,y2,x21,x22)
#define grcg_polygon_c(pts,np)			vgc_polygon_c(pts,np)
#define grcg_polygon_cx(pts,np)			vgc_polygon_cx(pts,np)
#define grcg_polygon_vcx				vgc_polygon_vcx

#define grcg_triangle(x1,y1,x2,y2,x3,y3) vgc_triangle(x1,y1,x2,y2,x3,y3)
#define grcg_triangle_x(pts)			vgc_polygon_cx(pts,3)	/* ? */

#define grcg_hline(x1,x2,y)				vgc_hline(x1,x2,y)
#define grcg_vline(x,y1,y2)				vgc_vline(x,y1,y2)
#define grcg_line(x1,y1,x2,y2)			vgc_line(x1,y1,x2,y2)
#define grcg_thick_line(x1,y1,x2,y2,wid,hei)	vgc_thick_line(x1,y1,x2,y2,wid,hei)
#define grcg_boxfill(x1,y1,x2,y2)		vgc_boxfill(x1,y1,x2,y2)
#define grcg_pset(x,y)					vgc_pset(x,y)
#define grcg_fill()						vgc_boxfill(ClipXL,ClipYT,ClipXR,ClipYB) /* ? */
#define grcg_round_boxfill(x1,y1,x2,y2,r) vgc_round_boxfill(x1,y1,x2,y2,r)

#define grcg_circle(x,y,r)				vgc_circle(x,y,r)
#define grcg_circle_x(x,y,r)			vgc_circle_x(x,y,r)
#define grcg_circlefill(x,y,r)			vgc_circlefill(x,y,r)

#define grcg_byteboxfill_x(x1,y1,x2,y2)	vgc_byteboxfill_x_pset(x1,y1,x2,y2)
#define grcg_bytemesh_x(x1,y1,x2,y2)	vgc_bytemesh_x(x1,y1,x2,y2)

#define GDC_PSET		VGA_PSET
#define GDC_XOR			VGA_XOR
#define GDC_AND			VGA_AND
#define GDC_OR			VGA_OR

#define SEQ_PORT			0x3c4
#define SEQ_MAP_MASK_REG	2

#define gdc_waitempty()				(void)0
#define gdc_wait()					(void)0
#define gdc_line(x1,y1,x2,y2)		vgc_line2(x1,y1,x2,y2,GDC_LineStyle)
#define gdc_circle(x,y,r)			vgc_circle(x,y,r)
#define gdc_setlinestyle(style) (GDC_LineStyle = (style))
#define gdc_setcolor(color)			vgc_setcolor((color) & ~15, (color)&15)
#define gdc_setaccessplane(plane)	OUTW(SEQ_PORT,SEQ_MAP_MASK_REG|(((~(plane))&15)<<8))


#define GRAM_400	0xa000
#define GRAM_200A	0xa000
#define GRAM_200B	0xa3e8	/* ? */

#endif

#endif

/* BEEP音による演奏処理 ----------------------------------------------------*/
/* 出典: bgmlib.h (C) femy, steelman
*/

#define BGM_STAT_PLAY	1 /* リターンコード */
#define BGM_STAT_MUTE	0
#define BGM_STAT_REPT	1
#define BGM_STAT_1TIM	0
#define BGM_STAT_ON		1
#define BGM_STAT_OFF	0

#define BGM_MES_ON	1 /* パラメータ */
#define BGM_MES_OFF	0
#define BGM_MUSIC	1
#define BGM_SOUND	2

#define BGM_COMPLETE	NoError /* エラーナンバー */
#define BGM_FILE_ERR	FileNotFound
#define BGM_FORMAT_ERR	InvalidFormat
#define BGM_OVERFLOW	InsufficientMemory
#define BGM_TOOMANY		InvalidData
#define BGM_NO_MUSIC	InvalidData
#define BGM_NOT_PLAY	GeneralFailure
#define BGM_NOT_STOP	GeneralFailure
#define BGM_EXTENT_ERR	InvalidData

#define BGM_PART_MAX	3
#define BGM_SOUND_MAX	16
#define BGM_MUSIC_MAX	16

typedef struct BSTAT BSTAT ;	/* ステータス構造体 */
struct BSTAT {
	int music;				/* 演奏処理 ON/OFF */
	int sound;				/* 効果音処理 ON/OFF */
	int play;				/* 演奏中か否か */
	int effect;				/* 効果音出力中か否か */
	int repeat;				/* リピート演奏か否か */
	int mnum;				/* 登録曲数 */
	int rnum;				/* セレクト中曲番号 */
	int tempo;				/* 現在のテンポ */
	int snum;				/* 登録効果音数 */
	int fnum;				/* セレクト中効果音番号 */
} ;

typedef struct BGM_GLB BGM_GLB;
struct BGM_GLB {	/* グローバルデータ構造体 */
	int		imr;				/* インターラプト・マスク・レジスタ */
	unsigned int	tval;		/* タイマ設定値 */
	int		tp;					/* テンポ */
	int		rflg;				/* BGM ON/OFF */
	int		pnum;				/* パート数 */
	int		pcnt;				/* パートカウンタ */
	int		fin;				/* パート終了フラグ */
	int		rep;				/* リピート ON/OFF */
	int		tcnt;				/* 処理カウンタ（4回に 1回処理 */
	int		bufsiz;				/* 楽譜バッファのサイズ */
	int		buflast;			/* 楽譜バッファのラスト */
	int		mnum;				/* 登録曲数 */
	int		mcnt;				/* セレクト中曲番号 */
	int		track[BGM_MUSIC_MAX]; /* 曲のトラックナンバー */
	int		mask;				/* パートマスク情報 */
	int		mtp[BGM_MUSIC_MAX];	/* 曲ごとのテンポ情報 */
	int		effect;				/* 効果音 ON/OFF */
	int		snum;				/* 登録効果音数 */
	int		scnt;				/* セレクト中効果音番号 */
	int		music;				/* 演奏処理 ON/OFF */
	int		sound;				/* 効果音処理 ON/OFF */
	int		init;				/* イニシャライズ実行フラグ */
	unsigned long	clockbase;	/* tempo120時のタイマカウント */
};

typedef struct BGM_PART BGM_PART;
struct BGM_PART {	/* パート構造体 */
	unsigned char far *ptr;		/* 楽譜ポインタ */
	unsigned char far *mbuf;	/* 楽譜バッファ */
	char	note;				/* 現在の音符 */
	char	_dummy;
	int		oct;				/* 現在のオクターブ */
	int		len;				/* 現在の音長 */
	int		dflen;				/* デフォルトの音長 */
	int		lcnt;				/* 音長カウンタ */
	int		mask;				/* パートマスク ON/OFF */
	int		tnt;				/* テヌート ON/OFF */
};

typedef struct BGM_ESOUND BGM_ESOUND;
struct BGM_ESOUND {	/* 効果音構造体 */
	unsigned int far *sptr;		/* 効果音データポインタ */
	unsigned int far *sbuf;		/* 効果音データバッファ */
};

extern BGM_GLB bgm_glb;
extern BGM_PART bgm_part[BGM_PART_MAX];
extern BGM_ESOUND bgm_esound[BGM_SOUND_MAX];

int MASTER_RET bgm_init(int bufsiz);
void MASTER_RET bgm_finish(void);
#define bgm_start(b)	bgm_init(b)
#define bgm_end()		bgm_finish()

int MASTER_RET bgm_read_data(const char MASTER_PTR *fname, int tempo, int mes);

int MASTER_RET bgm_select_music(int num);
int MASTER_RET bgm_start_play(void);
int MASTER_RET bgm_stop_play(void);
int MASTER_RET bgm_cont_play(void);
int MASTER_RET bgm_wait_play(void);
int MASTER_RET bgm_read_sdata(const char MASTER_PTR *fname);
int MASTER_RET bgm_sound(int num);
int MASTER_RET bgm_stop_sound(void);
int MASTER_RET bgm_wait_sound(void);
void MASTER_RET bgm_repeat_on(void);
void MASTER_RET bgm_repeat_off(void);
int MASTER_RET bgm_set_tempo(int tempo);
void MASTER_RET bgm_set_mode(int mode);
void MASTER_RET bgm_read_status(BSTAT MASTER_PTR *bsp);


/* 日本語FEP制御 -----------------------------------------------------------*/

#define FEP_IAS	 	1	/* fep_exist() */
#define FEP_MSKANJI	2	/* fep_exist() */

int MASTER_RET fep_exist(void);
int MASTER_RET fep_shown(void);
void MASTER_RET fep_show(void);
void MASTER_RET fep_hide(void);


/* 数値計算関連 ------------------------------------------------------------*/
extern const short SinTable8[256], CosTable8[256] ;
extern const char AtanTable8[256] ;
extern const short SinTableDeg[360], CosTableDeg[360] ;
extern const char AtanTableDeg[360] ;
extern long random_seed ;

#define Sin8(t) SinTable8[(t) & 0xff]
#define Cos8(t) CosTable8[(t) & 0xff]
#define Atan8(t) AtanTable8[t]

#define SinDeg(t)	(SinTableDeg[t])
#define CosDeg(t)	(CosTableDeg[t])
#define AtanDeg(t)	(AtanTableDeg[t])

int MASTER_RET iatan2( int y, int x );
int MASTER_RET iatan2deg( int y, int x );
int MASTER_RET isqrt( long x );
int MASTER_RET ihypot( int x, int y );

#define irand_init(seed)	(random_seed = (seed))
int MASTER_RET irand(void);

#define srand(s)	irand_init(s)
#define rand()		irand()

/* パックファイル処理 ------------------------------------------------------*/
/* 出典: pf.h (C) iR, alty
*/
#define __PF_H

typedef unsigned bf_t;					/* BFILE構造体のセグメント */
typedef unsigned pf_t;					/* PFILE構造体のセグメント */

/* エラー情報 */
extern enum pferr {
	PFEZERO,							/* 初期値 */
	PFENOTOPEN,							/* ファイルをオープンできない */
	PFENOTFOUND,						/* ファイルがパックファイル中にない */
	PFENOMEM,							/* メモリを確保できない */
	PFERESERVE0,						/* 予約 */
	PFEUNKNOWN,							/* 圧縮タイプが不正 */
#define PFEILTYPE PFEUNKNOWN
	PFEILPFILE,							/* パックファイルがpar形式でない */
	PFEILEXE,							/* EXEファイルが不正 */
	PFEINTERNAL = 0xff					/* 内部エラー */
} pferrno;

extern unsigned bbufsiz;				/* バッファサイズ */
#define BBUFSIZ 512
#define pfbufsiz bbufsiz

extern unsigned char pfkey;				/* 復号化キー */

bf_t MASTER_RET bopenr(const char *fname);
void MASTER_RET bcloser(bf_t bf);
int MASTER_RET bgetc(bf_t bf);
int MASTER_RET bread(void MASTER_PTR *buf, int size, bf_t bf);
int MASTER_RET bseek(bf_t bf, long offset);
int MASTER_RET bseek_(bf_t bf, long offset, int whence);
bf_t MASTER_RET bopenw(const char MASTER_PTR *fname);
void MASTER_RET bclosew(bf_t bf);
int MASTER_RET bputc(int c, bf_t bf);
int MASTER_RET bputw(int w, bf_t bf);
int MASTER_RET bputs(const char MASTER_PTR *s, bf_t bf);
int MASTER_RET bwrite(const void MASTER_PTR *buf, int size, bf_t bf);
int MASTER_RET bflush(bf_t bf);
bf_t MASTER_RET bdopen(int handle);
#define bdopenr bdopen
#define bdopenw bdopen
#define bsetbufsiz(bufsiz) (bbufsiz = (bufsiz))

pf_t MASTER_RET pfopen(const char MASTER_PTR *parfile, const char far *file);
#define pfopenr pfopen
void MASTER_RET pfclose(pf_t pf);
#define pfcloser pfclose
int MASTER_RET pfgetc(pf_t pf);
int MASTER_RET pfgetw(pf_t pf);
unsigned MASTER_RET pfread(void far *buf, unsigned size, pf_t pf);
unsigned long MASTER_RET pfseek(pf_t pf, unsigned long offset);
#define pftell(pf)	pfseek(pf,0)
void MASTER_RET pfrewind(pf_t pf);
#define pfsetbufsiz(bufsiz) bsetbufsiz(bufsiz)
void MASTER_RET pfstart(const char MASTER_PTR *parfile);
void MASTER_RET pfend(void);
#define pfinit pfstart
#define pfterm pfend

#ifdef __cplusplus
}
#endif

#endif
