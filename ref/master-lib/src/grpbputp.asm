; master library module
;
; Description:
;	グラフィック画面へのBFNT,パスカル文字列描画
;
; Functions/Procedures:
;	procedure graph_bfnt_putp(x,y,step:integer; str:string; color:integer);
;
; Parameters:
;	x,y	描画開始左上座標
;	step	文字ごとに進めるドット数(0=進めない)
;	str	パスカル文字列
;	color	色(0～15)
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;	$Id: ankputs.asm 0.01 92/06/06 14:45:43 Kazumi Rel $
;
;	93/ 2/25 Initial: master.lib <- super.lib 0.22b
;	93/ 7/23 [M0.20] far文字列にやっと対応(^^;
;	                 あとgraph_VramSeg使うようにした
;	94/ 1/24 Initial: grpbputs.asm/master.lib 0.22
;	94/ 1/24 Initial: grpbputp.asm/master.lib 0.22
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE
SCREEN_XBYTE equ 80

func GRAPH_BFNT_PUTP	; graph_bfnt_putp() {
	enter	12,0
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	pstr	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	xlen	= -1
	ylen	= -2
	x_add	= -4
	patmul	= -6
	yadd	= -8
	patseg	= -10
	slen	= -12

	mov	AX,font_AnkSeg
	mov	[BP+patseg],AX
	mov	ES,graph_VramSeg
	mov	AX,font_AnkSize
	mov	[BP+ylen],AX
	mov	AL,AH
	mov	AH,0
	sub	AX,SCREEN_XBYTE-1
	neg	AX
	mov	[BP+x_add],AX		; x_add = SCREEN_XBYTE-xlen-1
	mov	AL,SCREEN_XBYTE
	mul	byte ptr [BP+ylen]
	mov	[BP+yadd],AX		; yadd = ylen * SCREEN_XBYTE
	mov	AX,font_AnkPara
	mov	[BP+patmul],AX

	CLD
	_lds	SI,[BP+pstr]
	mov	BX,[BP+color]

	; 色の設定
	mov	AL,0c0h		;RMW mode
	pushf
	CLI
	out	7ch,AL
	popf
	REPT 4
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL
	ENDM

	; 文字数をみる
	lodsb
	or	AL,AL
	jz	short RETURN	; 文字列が空ならなにもしないぜ

	mov	CX,[BP+x]
	mov	DI,[BP+y]

	mov	AH,0
	mov	[BP+slen],AX
	mov	AX,SCREEN_XBYTE
	mul	DI
	mov	DI,AX
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	EVEN
LOOPTOP:			; 文字列のループ
	lodsb
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
	stosb
	dec	CH
	jnz	short DRAWX
	mov	AL,AH
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

	dec	word ptr [BP+slen]
	jnz	short LOOPTOP

RETURN:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(4+DATASIZE)*2
endfunc			; }

END
