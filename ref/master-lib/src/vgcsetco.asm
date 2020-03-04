; master library - grcg - PC98V - CINT
;
; Function:
;	void _pascal vgc_setcolor( int gmask, int color ) ;
;
; Description:
;	・VGCの色を設定する。
;
; Parameters:
;	int gmask	描画プレーンと演算を指定
;			bit0..3: 描画禁止マスク(1=禁止)
;			bit12-11:
;				0-0: pset
;				0-1: and
;				1-0: or
;				1-1: xor
;	int color	色。0..15
;
; Binding Target:
;	Microsoft-C / Turbo-C / etc...
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: V30
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0互換ならば OK )
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/ 3 Initial: vgcsetco.asm/master.lib 0.22

	.MODEL SMALL
	.CODE
	include func.inc
	include vgc.inc

func VGC_SETCOLOR	; vgc_setcolor() {
	mov	BX,SP

	; 引数
	gmask = (RETSIZE+1)*2
	color = (RETSIZE+0)*2

	mov	DX,VGA_PORT

	mov	AL,VGA_SET_RESET_REG
	mov	AH,SS:[BX+color]
	out	DX,AX

	mov	CX,SS:[BX+gmask]
if 0
	mov	AL,VGA_ENABLE_SR_REG
	mov	AH,CL
	not	AH
	out	DX,AX
endif

	mov	AL,VGA_DATA_ROT_REG
	mov	AH,CH
	out	DX,AX

	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX

	mov	DX,SEQ_PORT
	mov	AL,SEQ_MAP_MASK_REG
	mov	AH,CL
	not	AH
	out	DX,AX

	ret	4
endfunc			; }

END
