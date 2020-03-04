; master library - VGA - 16color
;
; Description:
;	VGA 16color 円描画(クリッピングなし)
;
; Function/Procedures:
;	void vgc_circle_x( int x, int y, int r ) ;
;
; Parameters:
;	x,y	中心座標
;	r	半径 (1以上)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16color
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	半径が 0 ならば描画しません。
;
; Notes:
;	・色をつけるには、vgc_setcolor()を利用してください。
;	・grc_setclip()によるクリッピングに対応しています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 5/12 Initial:gc_circl.asm/master.lib 0.16
;	93/ 5/14 加速
;	94/ 3/ 7 Initial for VGA(by Ara)
;	94/ 4/ 4 xorのために円弧の8分の1部分の重複描画を避けるようにした。
;	94/ 5/21 横640dot以外に対応(手抜き)
;	95/ 2/20 縦倍モード対応

	.186
	.MODEL SMALL

	.DATA

	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD
	EXTRN	graph_VramZoom:WORD

	.CODE
	include func.inc
	include	vgc.inc

func VGC_CIRCLE_X	; vgc_circle_x() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	r	= (RETSIZE+1)*2

	mov	DX,[BP+r]	; DX = 半径
	; 半径が 0 なら描画しない
	test	DX,DX
	jz	short RETURN

	mov	AL,byte ptr graph_VramZoom
	mov	CS:_VRAMSHIFT1,AL
	mov	CS:_VRAMSHIFT2,AL

	mov	AX,[BP+y]
	mov	BX,[BP+x]

	mov	CS:_cx_,BX
	mov	CS:_cy_1,AX
	mov	CS:_cy_2,AX
	mov	AX,graph_VramWidth
	mov	CS:MUL1,AX
	mov	CS:MUL2,AX
	mov	CS:MUL3,AX
	mov	CS:MUL4,AX
	mov	DS,graph_VramSeg
	xor	AX,AX
	mov	BP,DX
	jmp	short LOOPSTART
QUICKRETURN:
	pop	BP
	EVEN
RETURN:
	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6
	EVEN

LOOPTOP:
	stc		; BP -= AX*2+1;
	sbb	BP,AX	;
	sub	BP,AX	;
	jns	short LOOPCHECK
	dec	DX	; --DX;
	add	BP,DX	; BP += DX*2;
	add	BP,DX	;
LOOPCHECK:
	inc	AX	; ++AX;
	cmp	DX,AX
	jl	short RETURN
LOOPSTART:
	push	BP
NEXTDRAW:
	JMOV	BP,_cx_		; BP = 中心のx

	; in: DX = dx
	;     AX = dy
	mov	DI,AX
	shr	DI,9
	org	$-1
_VRAMSHIFT1	db	?

	JMOV	BX,_cy_1
	mov	SI,BX
	sub	SI,DI
	add	DI,BX

	imul	SI,SI,1234		; SI = (中心のy-dy) * 80
	org $-2
MUL1	dw	?
	imul	DI,DI,1234		; DI = (中心のy+dy) * 80
	org $-2
MUL2	dw	?

	mov	BX,BP
	sub	BX,DX	; BX = BP-DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,DX	; BX = BP+DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	cmp	AX,DX			; ここで 8分の1地点の2重描画を避ける
	je	short QUICKRETURN

	; in: AX = dx
	;     DX = dy
	mov	DI,DX
	shr	DI,9
	org	$-1
_VRAMSHIFT2	db	?

	JMOV	BX,_cy_2
	mov	SI,BX
	sub	SI,DI
	add	DI,BX

	imul	SI,SI,1234
	org $-2
MUL3	dw	?
	imul	DI,DI,1234
	org $-2
MUL4	dw	?

	mov	BX,BP
	sub	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH
	pop	BP
	jmp	LOOPTOP
endfunc			; }

END
