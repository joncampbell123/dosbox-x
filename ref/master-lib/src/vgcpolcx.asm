; master library - polygon - convex - nonclip - VGA
;
; Description:
;	凸多角形の描画 高速（機能削減）版
;
; Funciton/Procedures:
;	void vgc_polygon_cx( const Point PTRPAT * pts, int npoint ) ;
;
; Parameters:
;	Point * pts	座標リスト
;	int npoint	座標数
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Notes:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・クリッピングは一切行っていません。grc_clip_polygon_n()を利用して
;	　データのクリッピングを先に行ってから呼んで下さい。
;	・データが凸多角形になっていないと、正しく描画しません。
;	・npoint が 0 以下の場合、描画しません。
;	・npoint が 1～2 の場合、vgc_lineを呼び出します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;	grc_clip_polygon_n()
;
; Revision History:
;	92/ 7/11 Initial
;	92/ 7/18 点が少ないときの処理
;	93/ 6/28 2点以下のとき、large modelでおかしかったかも
;	94/ 4/ 9 Initial: vgcpolcx.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	.DATA?

	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	.CODE
	include func.inc

; make_linework - DDA LINE用構造体の作成
; IN:
;	DS:BX : LINEWORK * w ;	書き込み先
;	DX :	int x1 ;	上点のx
;	AX :	int x2 ;	下の点のx
;	CX :	int y2 - y1 ;	上下のyの差
	EXTRN	make_linework:NEAR

; vgc_draw_trapezoidx - 台形の描画 高速（機能削減）版
; IN:
;	SI : unsigned yadr	; 描画する三角形の上のラインの左端のVRAMｵﾌｾｯﾄ
;	DX : unsigned ylen	; 描画する三角形の上下のライン差(y2-y1)
;	ES : unsigned gseg	; 描画するVRAMのセグメント(graph_VramSeg)
;	trapez_a		; 側辺a(必ず左)のxの初期値と傾き
;	trapez_b		; 側辺b(必ず右)のxの初期値と傾き
	EXTRN	vgc_draw_trapezoidx:NEAR

	EXTRN	VGC_LINE:callmodel

IF datasize EQ 2
MOVPTS	macro	to, from
	mov	to,ES:from
	endm
PUSHPTS	macro	from
	push	ES:from
	endm
ESPTS	macro
	mov	ES,[BP+pts+2]
	endm
ESVRAM	macro
	mov	ES,graph_VramSeg
	endm
ELSE
MOVPTS	macro	to, from
	mov	to,from
	endm
PUSHPTS	macro	from
	push	from
	endm
ESPTS	macro
	endm
ESVRAM	macro
	endm
ENDIF
MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	(DATASIZE+1)*2
	EVEN
	endm

func VGC_POLYGON_CX	; vgc_polygon_cx() {
	enter	12,0
	push	SI
	push	DI

	; 引数
	pts	= (RETSIZE+2)*2
	npoint	= (RETSIZE+1)*2

	; ローカル変数
	nl = -10
	nr = -12
	ly = -4
	ry = -2
	ty = -6
	by = -8

if datasize eq 1
	mov	ES,graph_VramSeg
	mov	DI,[BP+pts]
else
	les	DI,[BP+pts]
endif

	; 頂点を探す～
	mov	SI,[BP+npoint]	; SI=npoint-1
	dec	SI
	js	short RETURN
	mov	[BP+nr],SI
	mov	DX,SI		; DX:nl
	mov	AX,SI
	shl	AX,2
	add	DI,AX
	MOVPTS	BX,[DI+2]	; BX:ty
	mov	CX,BX		; CX:by
	dec	SI
	jg	short LOOP1	; ...LOOP1E

	; 1～2点なら直線
	PUSHPTS	[DI]		; pts[npoint-1].x
	push	BX		; pts[npoint-1].y
	mov	DI,[BP+pts]
	PUSHPTS	[DI]		; pts[0].x
	PUSHPTS	[DI+2]		; pts[0].y
	call	VGC_LINE
RETURN:
	MRETURN

LOOP1:
	sub	DI,4
	MOVPTS	AX,[DI+2]
	cmp	AX,BX		; ty
	je	short L1_010
	jg	short L1_020
	mov	[BP+nr],SI
	mov	DX,SI		; nl
	mov	BX,AX		; ty
	jmp	short L1_030
L1_010:
	mov	DX,SI		; nl
	jmp	short L1_030
L1_020:
	cmp	CX,AX		; by
	jge	short L1_030
	mov	CX,AX		; by
L1_030:
	dec	SI
	jns	short LOOP1
LOOP1E:
	or	DX,DX		; nl
	jne	short L1_100
	mov	AX,[BP+npoint]
	dec	AX
	cmp	AX,[BP+nr]
	jne	short L1_100
	mov	DX,AX
	mov	WORD PTR [BP+nr],0
L1_100:
	mov	[BP+by],CX
	mov	DI,[BP+pts]
	; 最初の左側辺データを作成
	mov	BX,DX		; BX=nl
	dec	BX
	jge	short L2_010
	mov	BX,[BP+npoint]
	dec	BX
L2_010:
	mov	[BP+nl],BX
	shl	BX,2
	add	BX,DI
	mov	SI,DX
	shl	SI,2
	add	SI,DI
	MOVPTS	CX,[BX+2]
	mov	[BP+ly],CX
	MOVPTS	AX,[SI+2]
	mov	[BP+ty],AX
	MOVPTS	DX,[SI]
	sub	CX,AX
	MOVPTS	AX,[BX]
	mov	BX,offset trapez_a
	call	make_linework

	; 最初の右側辺データを作成
	mov	SI,[BP+nr]
	mov	DX,SI		; DX
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L3_010
	xor	SI,SI
L3_010:
	mov	BX,SI
	mov	[BP+nr],SI
	shl	BX,2
	MOVPTS	AX,[BX+DI+2]
	mov	[BP+ry],AX
	mov	CX,BX
	mov	BX,DX
	shl	BX,2
	MOVPTS	DX,[BX+DI]
	mov	BX,CX

	; 実際に描画していくんだ[よーん]
L4_010:
	sub	AX,[BP+ty]
	mov	CX,AX
	MOVPTS	AX,[BX+DI]
	mov	BX,offset trapez_b
	call	make_linework
	mov	[BP+nr],SI
LOOP4:
	mov	SI,[BP+ly]
	cmp	SI,[BP+ry]
	jle	short L4_020
	mov	SI,[BP+ry]
L4_020:
	cmp	[BP+by],SI
	jle	short LAST_ZOID
	push	SI
	push	DI
	lea	BX,[SI-1]
	xchg	SI,[BP+ty]
	sub	BX,SI
	xchg	AX,SI
	imul	graph_VramWidth
	xchg	SI,AX
	mov	DX,BX
	ESVRAM
	call	vgc_draw_trapezoidx		; 一番下でない台形
	ESPTS
	pop	DI
	pop	SI

	cmp	SI,[BP+ly]
	jne	short L4_040

	; 左側の側辺
	mov	CX,[BP+nl]
	mov	BX,CX
	shl	BX,2
	MOVPTS	DX,[BX+DI]	; DX!
	dec	CX
	jns	short L4_030
	mov	CX,[BP+npoint]
	mov	BX,CX
	shl	BX,2
	dec	CX
L4_030:
	mov	[BP+nl],CX
	sub	BX,4
	add	BX,DI
	MOVPTS	CX,[BX+2]
	mov	[BP+ly],CX
	sub	CX,[BP+ty]
	MOVPTS	AX,[BX]
	mov	BX,OFFSET trapez_a
	call	make_linework

L4_040:
	mov	AX,[BP+ry]
	cmp	[BP+ty],AX
	jne	short LOOP4

	; 右側の側辺
	mov	SI,[BP+nr]
	mov	BX,SI
	shl	BX,2
	MOVPTS	DX,[BX+DI]	; DX!
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L4_050
	xor	SI,SI
L4_050:
	mov	BX,SI
	shl	BX,2
	MOVPTS	AX,[BX+DI+2]
	mov	[BP+ry],AX
	jmp	L4_010

LAST_ZOID:
	; 一番下の台形
	ESVRAM
	mov	AX,[BP+ty]
	mov	BX,[BP+by]
	sub	BX,AX
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,BX
	call	vgc_draw_trapezoidx
	MRETURN
endfunc			; }
END
