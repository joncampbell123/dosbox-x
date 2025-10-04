; master library module - VGA
;
; Description:
;	グラフィック画面へのBFNT文字列描画
;
; Functions/Procedures:
;	void vgc_bfnt_puts( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	描画開始左上座標
;	step	文字ごとに進めるドット数(0=進めない)
;	anks	半角文字列
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
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
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

func VGC_BFNT_PUTS	; vgc_bfnt_puts() {
	enter	12,0
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	anks	= (RETSIZE+1)*2

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
	not	AX
	mov	[BP+x_add],AX		; x_add = graph_VramWidth-xlen-1
	mov	AX,graph_VramWidth
	mov	[BP+swidth],AX
	mul	byte ptr [BP+ylen]
	mov	[BP+yadd],AX		; yadd = ylen * graph_VramWidth
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

	EVEN
LOOPTOP:			; 文字列のループ
	push	DS
	mul	byte ptr [BP+patmul]
	add	AX,[BP+patseg]
	mov	DS,AX		; ank seg
	xor	BX,BX

	; 文字の描画
	mov	DH,[BP+ylen]
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
	test	ES:[DI],AL
	stosb
	dec	CH
	jnz	short DRAWX
	mov	AL,AH
	test	ES:[DI],AL
	stosb

	add	DI,[BP+x_add]
	dec	DH
	jnz	short DRAWY

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

RETURN:

	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(3+DATASIZE)*2
endfunc			; }

END
