; superimpose & master library module
;
; Description:
;	�p�^�[���f�[�^�̈�
;
; Variables:
;	
;
; Notes:
;	unsigned super_patdata[MAXPAT] ;
;	unsigned super_patsize[MAXPAT] ;
;	unsigned super_buffer ;
;	unsigned super_patnum ;
;	void (near *super_charfree)(void);	�S�L�����N�^�J���֐�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;	93/ 3/23 Initial: master.lib <- super.lib 0.22b superpat.asm
;	93/12/ 6 [M0.22] super_buffer, super_patnum�� 0�ŏ�����
;

	.MODEL SMALL
	include super.inc

	.DATA?
	PUBLIC	super_patdata,_super_patdata
	PUBLIC	super_patsize,_super_patsize
	PUBLIC	super_buffer,_super_buffer
	PUBLIC	super_patnum,_super_patnum
	PUBLIC	super_charfree,_super_charfree

_super_patdata label word
super_patdata	DW	MAXPAT dup (?)
_super_patsize label word
super_patsize	DW	MAXPAT dup (?)

	.DATA
_super_buffer label word
super_buffer	DW	0
_super_patnum label word
super_patnum	DW	0

_super_charfree label word
super_charfree	DW	0

END
