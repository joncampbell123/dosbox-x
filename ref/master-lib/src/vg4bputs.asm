; master library module - VGA
;
; Description:
;	グラフィック画面へのBFNT文字列描画(上書き)
;
; Functions/Procedures:
;	void vga4_bfnt_puts( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	描画開始左上座標
;	step	文字ごとに進めるドット数(0=進めない)
;	anks	半角文字列
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
;	94/ 6/12 [M0.23] 加速～

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

MRETURN macro
	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(4+DATASIZE)*2
	EVEN
endm

retfunc RETURN
	MRETURN
endfunc

func VGA4_BFNT_PUTS	; vga4_bfnt_puts() {
	enter	12,0
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	anks	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	xlen	= -1
	ylen	= -2
	x_add	= -4
	patmul	= -6
	yadd	= -8
	patseg	= -10
	swidth  = -12

	mov	AX,font_AnkSeg
	mov	[BP+patseg],AX
	mov	ES,graph_VramSeg
	mov	AX,font_AnkSize
	mov	[BP+ylen],AX
	mov	AL,AH
	mov	AH,0
	sub	AX,graph_VramWidth
	neg	AX
	mov	[BP+x_add],AX		; x_add = SCREEN_XBYTE-xlen
	mov	AX,graph_VramWidth
	mov	[BP+swidth],AX
	mul	byte ptr [BP+ylen]
	mov	[BP+yadd],AX		; yadd = ylen * SCREEN_XBYTE
	mov	AX,font_AnkPara
	mov	[BP+patmul],AX

	CLD
	_lds	SI,[BP+anks]

	; 最初の文字は何かな
	lodsb
	or	AL,AL
	jz	short RETURN	; 文字列が空ならなにもしないぜ

	mov	CX,[BP+x]
	mov	DI,[BP+y]

	mov	BX,AX
	mov	AX,[BP+swidth]
	mul	DI
	mov	DI,AX
	mov	AX,BX
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	mov	BX,AX

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
	mov	AL,VGA_SET_RESET_REG
	out	DX,AX

	mov	AX,BX
	EVEN
LOOPTOP:			; 文字列のループ
	push	DS
	mul	byte ptr [BP+patmul]
	add	AX,[BP+patseg]
	mov	DS,AX		; ank seg
	xor	BX,BX

	; 文字の描画
	mov	DH,[BP+ylen]
if 0
	test	CL,7
	jz	short BYTEEVEN

DRAWY:
	mov	CH,[BP+xlen]
	mov	DL,0
DRAWX:
	mov	AH,0
	mov	AL,[BX]
	inc	BX
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
	dec	DH
	jnz	short DRAWY
	jmp	short NEXTCHAR
endif
	; 8dot境界にあるときは加速
BYTEEVEN:
	mov	DL,[BP+xlen]
	mov	AX,[BP+x_add]
	xchg	SI,BX
	mov	CH,0
	EVEN
BELOOP:
	mov	CL,DL
	rep	movsb
	add	DI,AX
	dec	DH
	jnz	short BELOOP
	mov	SI,BX

NEXTCHAR:
	sub	DI,[BP+yadd]
	add	CX,[BP+step]	;CH == 0!!
	mov	AX,CX
	and	CX,7
	sar	AX,3
	add	DI,AX

	pop	DS

	lodsb
	or	AL,AL
	jnz	short LOOPTOP
	MRETURN
endfunc			; }

END
