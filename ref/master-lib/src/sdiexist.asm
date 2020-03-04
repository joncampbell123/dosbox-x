; master library - PC98
;
; Description:
;	SDIの常駐を検査する。
;
; Function/Procedures:
;	int sdi_exist( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 = 存在しない
;		1 = 存在する
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
;	グローバル変数 _SdiExistsに戻り値と同じ値を格納します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN SdiExists : WORD

	.CODE

func SDI_EXIST
	mov	AX,37B0h
	int	21h
	sub	AX,4453h	; 強烈～～～
	neg	AX		; つまり、4453だったら 1,それ以外なら 0に
	sbb	AX,AX		; してるんだよん
	inc	AX
	mov	SdiExists,AX
	ret
endfunc

END
