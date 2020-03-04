/* master.lib 		98雑用ライブラリ
 * Version 0.22i
 *
 *	mjoy.h - master.lib を joy.lib用プログラムで使うヘッダファイル
 *
 *	master.lib 0.23:	(C) 1994 恋塚,Kazumi,steelman
 *	joy.lib 0.91:		(C) 1990 STUDIO☆FEMY, 1991-1993 metys
 *
 *  ○joy.libとの相違点
 *		・JOY_INFO 構造体の各メンバは joy.libでは 16bitですが、
 *		  mjoy.hでは 1bitです。このため、構造体の大きさも違います。
 *		  値を読むと、joy.libでは 0 以外の値は不定ですが、mjoy.hでは
 *		  0 以外は常に 1 です。
 *
 *		・TRIG1_1Pなどのキー割当指示定数は、joy.libでは int値ですが、
 *		  mjoy.hではメモリアドレスです。このため、これを計算で扱っている
 *		  プログラムは修正が必要です。
 *
 *		・joyKey2player(JOY_TRUE)を実行したあとに
 *		  joyKey2player(JOY_FALSE)を実行すると、
 *		  joy.libでは完全に元に戻りませんが、mjoy.hでは奇麗に戻ります。
 *
 *		・joy.libでは、2重includeや C++での使用を考慮していませんが、
 *		  mjoy.hでは対応しています。また、メモリモデルも master.lib の
 *		  ものがそのまま利用できます。
 */
#ifndef __MJOY_H
#define __MJOY_H

#ifndef __MASTER_H
# include "master.h"
#endif

#if __MASTER_VERSION < 23
# error master.lib 0.23以上が必要です!!
#endif

#define	JOY_COMPLETE	1
#define	JOY_ERROR		0

#define	TRIG1_1P		JS_1P1
#define	TRIG2_1P		JS_1P2
#define	IRST1_1P		JS_1P4
#define	IRST0_1P		JS_1P3
#define	UP_2P			JS_2PUP
#define	DOWN_2P			JS_2PDOWN
#define	LEFT_2P			JS_2PLEFT
#define	RIGHT_2P		JS_2PRIGHT
#define	TRIG1_2P		JS_2P1
#define	TRIG2_2P		JS_2P2

/* type definition */
typedef struct JOY_INFO {
	unsigned up:1 ;
	unsigned down:1 ;
	unsigned left:1 ;
	unsigned right:1 ;
	unsigned trig1:1 ;
	unsigned trig2:1 ;
	unsigned irst1:1 ;
	unsigned irst0:1 ;
	unsigned esc:1 ;
	unsigned dummy:7 ;
} JOY_INFO ;

/* function replacements */
#define joyInit(flag)	(js_start(flag) ? JOY_COMPLETE : JOY_ERROR)
#define	JOY_NORMAL		JS_NORMAL
#define	JOY_IGNORE		JS_IGNORE
#define	JOY_INITIALIZE	JS_FORCE_USE

#define	joyReadInfo(joy_data)	(js_sense(),\
						(*(long *)(joy_data) = *(long *)js_stat),\
						(joy_data[0].esc))
#define joyReadInfo2(joy_data)	(js_sense2(),\
						(*(long *)(joy_data) = *(long *)js_stat),\
						(joy_data[0].esc))

#define	joyKey2player(flag)		js_key2player(flag)
#define	JOY_TRUE		1
#define	JOY_FALSE		0
#define joyKeyAssign(func,group,mask)	js_keyassign((func),(group),(mask))
#define	joyAssign(n)	(js_shift=(char)(n))
#define	JOY_SHIFT		1

/* for backward compatibility */
#define joy_init(flag)			 joyInit(flag)
#define joy_read_info(joy_data)	 joyReadInfo(joy_data)
#define joy_read_info2(joy_data) joyReadInfo2(joy_data)
#define joy_key_2player(flag)	 joyKey2player(flag)
#define joy_key_assign(cf,kg,mb) joyKeyAssign((cf),(kg),(mb))
#define	joy_assign(n)			 (js_shift=(char)(n))

#define joySft	js_shift
#endif
