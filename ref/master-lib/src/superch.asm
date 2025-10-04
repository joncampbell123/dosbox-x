; superimpose & master library module
;
; Description:
;	キャラクタデータ領域
;
; Variables:
;	unsigned super_chardata[MAXCHAR] ;
;	unsigned super_charnum[MAXCHAR] ;
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 3/23 Initial: master.lib <- super.lib 0.22b superchr.asm
;

	.MODEL SMALL
	include super.inc

	.DATA?
	PUBLIC	super_chardata,_super_chardata
	PUBLIC	super_charnum,_super_charnum
	PUBLIC	super_charcount,_super_charcount

_super_chardata label word
super_chardata	DW	MAXCHAR dup (?)
_super_charnum label word
super_charnum	DW	MAXCHAR dup (?)

	.DATA
_super_charcount label word
super_charcount	DW	0

END
