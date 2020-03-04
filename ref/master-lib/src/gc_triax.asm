PAGE 98,120
; grcg - triangle - no_clipping
;
; Description:
;	三角形塗りつぶし(青プレーンのみ,クリッピングなし)
;
; Functions/Procedures:
;	void grcg_triangle_x( Point * pts )
;
; Parameters:
;	Point * pts	３要素の点の配列
;
; Binding Target:
;	Microsoft-C / Turbo-C / MS-PASCAL? / MS-FORTRAN?
;
; Running Target:
;	grcg_trapezoidx に依存
;
; Requiring Resources:
;	CPU: V30
;	grcg_trapezoidx に依存
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	(あらかじめgc_colorで色を設定してから呼びだし、終ったらgc_reset()で
;	 グラフィックチャージャーのスイッチを切る)
;	・クリッピングは行っていません。クリップ枠外を指定すると、
;	　実画面内でもハングアップする可能性大でーす
;	　　必ずデータがクリップ枠内であることが保証された状態で実行して
;	　下さい。
;
; 関連関数:
;	grcg_setcolor(), grcg_off()
;	grc_setclip()
;	make_linework
;	draw_trapezoidx
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
;	92/6/6	ClipYT_seg対応 = クリッピング対応完了
;	92/6/16 TASM対応
;	93/3/2  クリップなし版, 引数は Point *に変更

	.186
	.MODEL SMALL

	; 台形
	EXTRN make_linework:NEAR
	EXTRN draw_trapezoidx:NEAR
	EXTRN trapez_a:WORD
	EXTRN trapez_b:WORD

	; クリップ枠
	EXTRN ClipYT:WORD
	EXTRN ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	DATASIZE*2
	EVEN
	endm

;=========================================================================
; void grcg_triangle_x( Point * pts )
func GRCG_TRIANGLE_X	; {
	enter	10,0
	push	SI
	push	DI

	; 引数
	pts = (RETSIZE+1)*2

	; 変数
	x1   = -2
	x2   = -4
	y2   = -6
	x3   = -8
	y3   = -10

	_push	DS
	_lds	DI,[BP+pts]

	mov	AX,[DI]		; pts[0].x
	mov	BX,[DI+2]	; pts[0].y
	mov	CX,[DI+4]	; pts[1].x
	mov	DX,[DI+6]	; pts[1].y
	mov	SI,[DI+8]	; pts[2].x
	mov	DI,[DI+10]	; pts[2].y

	_pop	DS

	; ｙ座標順に並べ替え

	cmp	BX,DX		; y1,y2
	jle	short sort1
		xchg	AX,CX	; x1,x2
		xchg	BX,DX	; y1,y2
sort1:
	cmp	BX,DI		; y1,y3
	jl	short sort2
		xchg	AX,SI	; x1,x3
		xchg	BX,DI	; y1,y3
sort2:
	cmp	DX,DI		; y2,y3
	jle	short sort3
		xchg	CX,SI	; x2,x3
		xchg	DX,DI	; y2,y3
sort3:
	mov	[BP+x1],AX
	mov	[BP+x2],CX
	sub	DX,BX		; y2 -= y1
	mov	[BP+y2],DX
	mov	[BP+x3],SI
	sub	DI,BX		; y3 -= y1
	mov	[BP+y3],DI

	; 描画セグメントを設定する
	sub	BX,ClipYT
	mov	AX,BX		; ES = ClipYT_seg + (y1-ClipYT) * 5
	shl	AX,2
	add	AX,BX
	add	AX,ClipYT_seg
	mov	ES,AX

	mov	AX,SI		; AX = x3
	mov	CX,DI		; CX = DI = y3
	mov	BX,offset trapez_b
	mov	DX,[BP+x1]
	call	make_linework

	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	mov	CX,[BP+y2]

	jcxz	short SUIHEI_1		; y2 == y1
	cmp	DI,CX			; y3 == y2
	je	short SUIHEI_2

	; 水平な辺がない場合
DOUBLE:
	mov	[BP+y3],DI

	mov	DI,CX			; DI = y2
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	lea	DX,[DI-1]
	xor	SI,SI
	call	draw_trapezoidx
	mov	CX,[BP+y3]
	mov	AX,[BP+x3]
	sub	CX,[BP+y2]
	mov	DI,CX
	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	call	make_linework
	mov	DX,DI
	call	draw_trapezoidx
	MRETURN

; 上辺が水平な場合
SUIHEI_1:
	mov	AX,[BP+x3]
	mov	CX,DI			; y3 - y2 (y2 = y1 = 0だから引かない)
	call	make_linework
	EVEN
TAIL:
	xor	SI,SI
	mov	DX,[BP+y3]
	call	draw_trapezoidx
	MRETURN			; こんな所にもリターンが

; 底辺が水平な場合
SUIHEI_2:
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	push	offset TAIL
	jmp	make_linework

endfunc			; }
END
