; master library - PC98
;
; Description:
;	半角フォントパターンを組み込む
;
; Function/Procedures:
;	void font_link( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	tinyモデルでは使えません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 4/25 Initial: fontlink.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

FONTDATASEG segment para
FontData	db	0
		db	0	; ....それ以前にまだできてない
FONTDATASEG ends

	.DATA
	EXTRN font_AnkSeg:WORD

	.CODE

func FONT_LINK
	mov	font_AnkSeg,seg FontData
	ret
endfunc

END
