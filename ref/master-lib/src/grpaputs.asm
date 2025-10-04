; superimpose & master library module
;
; Description:
;	グラフィック画面への半角文字列描画
;
; Functions/Procedures:
;	void graph_ank_puts( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	描画開始左上座標
;	step	文字ごとに進めるドット数(0=進めない)
;	anks	半角文字列
;	color	色(0～15)
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	super.lib 0.22b の ank_putsに対応します。
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
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func GRAPH_ANK_PUTS
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	anks	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	DX,font_AnkSeg
	mov	ES,graph_VramSeg

	_lds	SI,[BP+anks]
	mov	BX,[BP+color]

	; 色の設定
	mov	AL,0c0h		;RMW mode
	pushf
	CLI
	out	7ch,AL
	popf
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL

	; 最初の文字は何かな
	lodsb
	or	AL,AL
	jz	short RETURN	; 文字列が空ならなにもしないぜ

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+step]
	mov	BP,BX

	mov	BX,DI		;-+
	shl	BX,2		; |
	add	DI,BX		; |DI=y*80
	shl	DI,4		;-+
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	xor	CH,CH

	EVEN
LOOPTOP:			; 文字列のループ
	push	DS
	xor	AH,AH
	add	AX,DX
	mov	DS,AX		; ank seg
	xor	BX,BX

	EVEN
ANK_LOOP:			; 文字の描画
	mov	AL,[BX]
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,78
	inc	BX
	cmp	BX,16
	jnz	short ANK_LOOP

	sub	DI,80 * 16
	add	CX,BP		;CH == 0!!
	mov	AX,CX
	and	CX,7
	shr	AX,3
	add	DI,AX

	pop	DS

	lodsb
	or	AL,AL
	jnz	short LOOPTOP

RETURN:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(4+DATASIZE)*2
endfunc

END
