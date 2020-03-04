; master library - PC-9801
;
; Description:
;	シフトキー押下状態の読み取り(関数版)
;
; Function/Procedures:
;	int key_shift(void) ;
;
; Parameters:
;	None
;
; Returns:
;	int 	1	SHIFT
;		2	CAPS
;		4	ｶﾅ
;		8	GRPH
;		16	CTRL
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
;	マクロ版だとワークエリア直接読み取りのため、キーボードの読み取りを
;	いじる TSR との相性がよくありません。
;	安全のため、関数版を使うべきです。
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
;	93/ 5/ 1 Initial:keyshift.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE

func KEY_SHIFT
	mov	AH,2
	int	18h
	mov	AH,0
	ret
endfunc

END
