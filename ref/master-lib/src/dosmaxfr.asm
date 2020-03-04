; master library - 
;
; Description:
;	DOS の最大フリーブロックのパラグラフサイズを得る
;
; Function/Procedures:
;	unsigned dos_maxfree(void) ;
;	function dos_maxfree : Word ;
;
; Parameters:
;	none
;
; Returns:
;	最大フリーブロックのパラグラフサイズ
;	0 ならメモリコントロールブロックが破壊されている
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/11/ 1 Initial: dosmaxfr.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_MAXFREE ; dos_maxfree() {
	mov	AH,48h
	mov	BX,-1
	int	21h
	cmp	AX,8	; メモリ不足?
	jne	short ERROREXIT
	mov	AX,BX
	clc
	ret
ERROREXIT:	; ここに来るとしたら、
		;メモリコントロールブロックが破壊されている場合かな
	xor	AX,AX
	stc
	ret
endfunc		; }

END
