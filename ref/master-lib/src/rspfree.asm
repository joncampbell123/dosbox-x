; master library - PC98
;
; Description:
;	常駐パレットの開放
;
; Function/Procedures:
;	void respal_free( void ) ;
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
;	常駐パレットが作成されていなければ、何もしません。
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
	EXTRN ResPalSeg:WORD

	.CODE
	EXTRN RESPAL_EXIST:CALLMODEL

func RESPAL_FREE
	mov	AX,ResPalSeg
	or	AX,AX
	jnz	short FREE
	call	RESPAL_EXIST
	or	AX,AX
	jnz	short IGNORE
FREE:
	mov	ES,AX
	mov	AH,49h	; メモリブロックの開放
	int	21h

	mov	ResPalSeg,0
IGNORE:
	ret
endfunc

END
