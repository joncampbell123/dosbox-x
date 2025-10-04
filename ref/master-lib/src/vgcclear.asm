; master library - VGA - 16color
;
; Description:
;	VGA16色, 画面の消去
;
; Function/Procedures:
;	void vga4_clear(void) ;
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
;	VGA
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	93/12/ 3 Initial: vgcclear.asm/master.lib 0.22
;	94/ 3/12 [M0.23] 全く動いてなかった(^^;

	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN graph_VramSeg : WORD
	EXTRN graph_VramWords : WORD

	.CODE

func VGA4_CLEAR	; vga4_clear() {
	push	DI

	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	out	DX,AX

	mov	DX,VGA_PORT

	mov	AX,VGA_SET_RESET_REG or (0 shl 8)
	out	DX,AX

	mov	AX,VGA_ENABLE_SR_REG or (0fh shl 8)
	out	DX,AX

	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX

	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX

	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX

	mov	ES,graph_VramSeg
	mov	CX,graph_VramWords

	CLD

	mov	DI,0
	mov	byte ptr ES:[DI],0	; 最初は mode 0でセットする
	scasb				; そのあと latch に読み込む

	; 次からラッチモードで塗る
	mov	AX,VGA_MODE_REG or (VGA_LATCH shl 8)
	out	DX,AX

	stosb
	dec	CX

	rep	stosw

	pop	DI
	ret
endfunc		; }

END
