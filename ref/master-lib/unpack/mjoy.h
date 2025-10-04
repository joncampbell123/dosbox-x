/* master.lib 		98�G�p���C�u����
 * Version 0.22i
 *
 *	mjoy.h - master.lib �� joy.lib�p�v���O�����Ŏg���w�b�_�t�@�C��
 *
 *	master.lib 0.23:	(C) 1994 ����,Kazumi,steelman
 *	joy.lib 0.91:		(C) 1990 STUDIO��FEMY, 1991-1993 metys
 *
 *  ��joy.lib�Ƃ̑���_
 *		�EJOY_INFO �\���̂̊e�����o�� joy.lib�ł� 16bit�ł����A
 *		  mjoy.h�ł� 1bit�ł��B���̂��߁A�\���̂̑傫�����Ⴂ�܂��B
 *		  �l��ǂނƁAjoy.lib�ł� 0 �ȊO�̒l�͕s��ł����Amjoy.h�ł�
 *		  0 �ȊO�͏�� 1 �ł��B
 *
 *		�ETRIG1_1P�Ȃǂ̃L�[�����w���萔�́Ajoy.lib�ł� int�l�ł����A
 *		  mjoy.h�ł̓������A�h���X�ł��B���̂��߁A������v�Z�ň����Ă���
 *		  �v���O�����͏C�����K�v�ł��B
 *
 *		�EjoyKey2player(JOY_TRUE)�����s�������Ƃ�
 *		  joyKey2player(JOY_FALSE)�����s����ƁA
 *		  joy.lib�ł͊��S�Ɍ��ɖ߂�܂��񂪁Amjoy.h�ł͊��ɖ߂�܂��B
 *
 *		�Ejoy.lib�ł́A2�dinclude�� C++�ł̎g�p���l�����Ă��܂��񂪁A
 *		  mjoy.h�ł͑Ή����Ă��܂��B�܂��A���������f���� master.lib ��
 *		  ���̂����̂܂ܗ��p�ł��܂��B
 */
#ifndef __MJOY_H
#define __MJOY_H

#ifndef __MASTER_H
# include "master.h"
#endif

#if __MASTER_VERSION < 23
# error master.lib 0.23�ȏオ�K�v�ł�!!
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
