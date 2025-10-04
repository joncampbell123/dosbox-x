; master library module - VGA
;
; Description:
;	グラフィック画面へのBFNT文字描画(上書き)
;
; Functions/Procedures:
;	void vga4_bfnt_putc( int x, int y, int c, int color ) ;
;
; Parameters:
;	x,y	描画開始左上座標
;	c	半角文字
;	color	上位4bit:背景色
;		下位4bit:文字色
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	$Id: ankputs.asm 0.01 92/06/06 14:45:43 Kazumi Rel $
;
;	93/ 2/25 Initial: master.lib <- super.lib 0.22b
;	93/ 7/23 [M0.20] far文字列にやっと対応(^^;
;	                 あとgraph_VramSeg使うようにした
;	94/ 1/24 Initial: grpbputs.asm/master.lib 0.22
;	94/ 4/11 Initial: vgcbputs.asm/master.lib 0.23
;	94/ 5/23 Initial: vg4bputs.asm/master.lib 0.23
;	94/ 6/ 5 Initial: vg4bputc.asm/master.lib 0.23
;	94/ 7/27 [M0.23] BUGFIX

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

func VGA4_BFNT_PUTC	; vga4_bfnt_putc() {
	enter	2,0
	push	SI
	push	DI
	push	DS

	; 引数
	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	c	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	x_add	= -2

	mov	ES,graph_VramSeg
	mov	SI,graph_VramWidth

	CLD
	mov	AX,font_AnkSize
	mov	AL,AH
	mov	AH,0
	sub	AX,SI
	neg	AX
	mov	[BP+x_add],AX		; x_add = graph_VramWidth-xlen

	mov	CX,[BP+x]
	mov	AX,[BP+y]
	mul	SI
	mov	DI,AX
	mov	BX,CX
	and	CX,7			; CL = shift count
	shr	BX,3
	add	DI,BX			; DI = vram offset

	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	out	DX,AX

	mov	DX,VGA_PORT
	mov	AH,[BP+color]		; background color
	shr	AH,4
	mov	AL,VGA_SET_RESET_REG
	out	DX,AX
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	mov	AL,0ffh
	mov	ES:[DI],AL	; write background
	test	ES:[DI],AL	; load latch

	mov	AH,[BP+color]		; foreground color
	and	AH,0fh
	mov	AL,VGA_SET_RESET_REG
	out	DX,AX

	mov	BX,font_AnkSize	; BL=ylen  BH=xlen
	mov	AX,font_AnkPara
	mul	byte ptr [BP+c]
	add	AX,font_AnkSeg
	mov	DS,AX		; ank seg
	xor	SI,SI

	; 文字の描画
DRAWY:
	mov	CH,BH	; xlen
	mov	DL,0
DRAWX:
	mov	AH,0
	lodsb
	ror	AX,CL
	or	AL,DL
	mov	DL,AH
	stosb
	dec	CH
	jnz	short DRAWX
	jcxz	short SKIP_RIGHT
	mov	ES:[DI],AH
SKIP_RIGHT:

	add	DI,[BP+x_add]
	dec	BL
	jnz	short DRAWY

	pop	DS
	pop	DI
	pop	SI
	leave
	ret	4*2
endfunc			; }

END
