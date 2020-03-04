; master library - MS-DOS
;
; Description:
;	DOS・標準入力からの文字読み取り２
;	(^Cで止まらない, 入力がなくても待たない)
;
; Function/Procedures:
;	int dos_getkey( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	1 .. 255 入力された文字
;		0 ...... 入力なし
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
;	キャラクタコード 0 の文字は入力なしと区別できません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/20 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETKEY
	mov	AH,6
	mov	DL,0FFh
	int	21h
	xor	AH,AH
	ret
endfunc

END
