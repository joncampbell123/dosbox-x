; master library - MS-DOS
;
; Description:
;	DOS高速コンソール文字出力
;
; Function/Procedures:
;	void dos_putch( int chr ) ;
;
; Parameters:
;	int chr		文字
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	DOSの特殊デバイスコール ( int 29h )を利用しています。
;	将来の DOSでは使えなくなるかもしれません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTCH
	mov	BX,SP
	mov	AL,SS:[BX+ RETSIZE*2]
	int	29h
	ret	2
endfunc

END
