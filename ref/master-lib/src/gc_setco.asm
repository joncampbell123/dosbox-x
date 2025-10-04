; master library - grcg - PC98V - CINT
;
; Function:
;	void _pascal grcg_setcolor( int mode, int color ) ;
;	void _pascal grcg_off(void) ;
;
; Description:
;	・グラフィックチャージャーのモード設定し、単色を設定する。
;	・グラフィックチャージャーのスイッチを切る。
;
; Parameters:
;	int mode	モード。
;	int color	色。0..15
;
; Binding Target:
;	Microsoft-C / Turbo-C / etc...
;
; Running Target:
;	PC-9801V Normal Mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRAPHIC CHAGER
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0互換ならば OK )
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/ 6/ 8 Initial
;	92/ 6/11 gc_setcolor()が"ret"で戻っていた(^^; -> ret 4 に修正
;		 (Thanks, Mikio)
;	92/ 6/16 TASM対応
;	92/ 7/10 mode reg設定時、割り込みを禁止
;	93/ 5/ 5 [M0.16] タイルレジスタ設定中まで割り込みを禁止したよーん

	.MODEL SMALL
	.CODE
	include func.inc

func GRCG_SETCOLOR	; {
	mov	BX,SP

	; 引数
	mode  = (RETSIZE+1)*2
	color = (RETSIZE+0)*2

	mov	AL,SS:[BX+mode]
	mov	AH,SS:[BX+color]

	mov	DX,007eh
	pushf
	CLI
	out	7ch,AL

	shr	AH,1		; B
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; R
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; G
	sbb	AL,AL
	out	DX,AL

	shr	AH,1		; I
	sbb	AL,AL
	out	DX,AL
	popf

	ret	4
endfunc			; }

; void _pascal grcg_off(void) ;
func GRCG_OFF
	xor	AL,AL
	out	7ch,AL
	ret
endfunc

END
