; master library - VGA - 16Color - triangle
;
; Function:
;	void vgc_triangle( int x1, int y1, int x2, int y2, int x3, int y3 ) ;
;
; Description:
;	三角形塗りつぶし
;
; Parameters:
;	int x1,y1		第１点の座標
;	int x2,y2		第２点の座標
;	int x3,y3		第３点の座標
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; RUNNING TARGET:
;	vgc_trapezoid に依存
;
; REQUIRING RESOURCES:
;	CPU: 186
;	vgc_trapezoid に依存
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・クリッピングはvgc_trapezoidに依っています。
;
; 関連関数:
;	gc_color(), gc_reset()
;	grc_setclip()
;	make_linework
;	vgc_draw_trapezoid
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/3/21 Initial
;	92/3/29 bug fix, 台形に側辺の交差を認めるようにした
;	92/4/2 少々加速
;	92/6/4	外の台形呼ぶようにしたら、仕様がちがった～ひー
;	92/6/5	↑ので直したのだ。
;	92/6/6	graph_VramSeg対応 = クリッピング対応完了
;	92/6/16 TASM対応
;	94/ 4/11 Initial: vgctrian.asm/master.lib 0.23

	.186
	.MODEL SMALL

	.DATA?
	; 台形
	EXTRN	trapez_a: WORD
	EXTRN	trapez_b: WORD

	; クリップ枠
	EXTRN	ClipYT: WORD
	EXTRN	ClipYB: WORD
	EXTRN	graph_VramSeg: WORD
	EXTRN	graph_VramWidth: WORD


	; 三角形描画用変数
ylen	dw	?

	.CODE
	include func.inc

	EXTRN	make_linework: NEAR
	EXTRN	vgc_draw_trapezoid: NEAR

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	12
	EVEN
	endm

func VGC_TRIANGLE	; vgc_triangle() {
	push	BP
	mov	BP,SP
	push	DI
	push	SI

	; パラメータ
	x1 = (RETSIZE+6)*2
	y1 = (RETSIZE+5)*2
	x2 = (RETSIZE+4)*2
	y2 = (RETSIZE+3)*2
	x3 = (RETSIZE+2)*2
	y3 = (RETSIZE+1)*2

	; 描画セグメントを設定する
	mov	ES,graph_VramSeg

	; ｙ座標順に並べ替え(とりあえず～)
	mov	AX,[BP+x1]
	mov	BX,[BP+y1]
	mov	CX,[BP+x2]
	mov	DX,[BP+y2]
	mov	SI,[BP+x3]
	mov	DI,[BP+y3]

	cmp	BX,DX		; y1,y2
	jl	short sort1
		xchg	AX,CX	; x1,x2
		xchg	BX,DX	; y1,y2
sort1:
	cmp	BX,DI		; y1,y3
	jl	short sort2
		xchg	AX,SI	; x1,x3
		xchg	BX,DI	; y1,y3
sort2:
	cmp	DX,DI		; y2,y3
	jl	short sort3
		xchg	CX,SI	; x2,x3
		xchg	DX,DI	; y2,y3
sort3:
	cmp	BX,ClipYB
	jg	short TAIL	; 完全に枠の下 → clip out
	cmp	DI,ClipYT
	jl	short TAIL	; 完全に枠の上 → clip out

	mov	[BP+x1],AX
	mov	[BP+y1],BX
	mov	[BP+x2],CX
	mov	[BP+y2],DX
	mov	[BP+x3],SI
	mov	[BP+y3],DI

	;
	; DI = y3
	; AX = x3
	mov	AX,SI
	; SI = y1
	mov	SI,BX

	mov	CX,DI			; y3
	mov	BX,offset trapez_b
	sub	CX,SI			; y1
	mov	ylen,CX
	mov	DX,[BP+x1]
	call	make_linework

	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	mov	CX,[BP+y2]

	cmp	CX,SI			; y2,y1
	jne	short CHECK2
; 上辺が水平な場合
	mov	AX,[BP+x3]
	sub	DI,CX			; y3 - y2
	mov	CX,DI			;
	call	make_linework
	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	mov	DX,ylen
	push	offset TAIL
	jmp	vgc_draw_trapezoid
	EVEN
TAIL:
	MRETURN			; こんな所にもリターンが

CHECK2:
	cmp	DI,CX			; y3,y2
	jne	short DOUBLE
; 底辺が水平な場合
	sub	CX,SI			; y1
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	mov	DX,ylen
	push	offset TAIL
	jmp	vgc_draw_trapezoid

	EVEN
; 水平な辺がない場合
DOUBLE:
	sub	CX,SI			; DI = y2 - y1
	mov	DI,CX			;
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	mov	AX,[BP+x2]
	cmp	[BP+x3],AX

	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	lea	DX,[DI-1]
	call	vgc_draw_trapezoid
	mov	CX,[BP+y3]
	mov	AX,[BP+x3]
	sub	CX,[BP+y2]
	mov	ylen,CX
	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	call	make_linework
	mov	DX,ylen
	call	vgc_draw_trapezoid
	MRETURN
endfunc			; }
END
