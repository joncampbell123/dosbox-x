; master library - VGA - polygon - convex - Point
;
; Description:
;	凸多角形描画
;
; Functions/:
;	void vgc_polygon_c( Point far pts[], int npoint ) ;
;
; PARAMETERS:
;	Point pts[]	座標リスト
;	int npoint	座標数( 2～ )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・クリッピング枠によって描画を制限できます。( grc_setclip()参照 )
;	・左右の境界クリップは、台形描画ルーチンに依存しています。
;	・データが下記の条件に当てはまる場合、正しく描画しません。
;	    ○同じy座標を経由する辺が2つを超える場所がある場合。
;		→条件により、ゼロ除算が発生したり、おかしな図形になります。
;		（凸多角形でなくても、上記以外なら正しく描画します）
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; 関連関数:
;	grc_setclip()
;	make_linework
;	vgc_draw_trapezoid
;
; Author:
;	恋塚昭彦
;
; HISTORY:
;	92/5/18 Initial(C版)
;	92/5/19 bugfix
;	92/5/20 とりあえず、左右と下端のクリップをやった。
;	92/5/21 左右の先判定と、上もクリップしたぞ。（あーのんびり。）
;	92/6/4	Initial(asm版 gc_4corn.asm)
;	92/6/5	gc_polyg.asm
;	92/6/6	ClipYT_seg対応
;	92/6/16 TASM対応
;	92/6/19 gc_polgc.asm
;	92/6/21 Pascal対応
;	94/ 4/ 9 Initial: vgcpolgc.asm/master.lib 0.23
;	94/ 7/26 [M0.23] エンバグによりsmall model以外で死んでたのを訂正

	.MODEL SMALL
	.186

	; 台形
	EXTRN	make_linework:NEAR
	EXTRN	vgc_draw_trapezoid:NEAR
	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	; クリップ枠
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	graph_VramWidth:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

	include FUNC.INC

IF datasize EQ 2
MOVPTS	macro	to, from
	mov	to,ES:from
	endm
CMPPTS	macro	pts, dat
	cmp	ES:pts,dat
	endm
SUBPTS	macro	reg,pts
	sub	reg,ES:pts
	endm
LODPTS	macro	reg
	les	reg,[BP+pts]
	endm
ELSE
MOVPTS	macro	to, from
	mov	to,from
	endm
CMPPTS	macro	pts, dat
	cmp	pts,dat
	endm
SUBPTS	macro	reg,pts
	sub	reg,pts
	endm
LODPTS	macro	reg
	mov	reg,[BP+pts]
	endm
ENDIF

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	(datasize+1)*2
	EVEN
	endm

func VGC_POLYGON_C	; vgc_polygon_c() {
	push	BP
	mov	BP,SP
	sub	SP,12	; ローカル変数のサイズ
	push	DI
	push	SI

	; パラメータ
	pts	= (RETSIZE+2)*2
	npoint	= (RETSIZE+1)*2

	; ローカル変数
	ry = -2		; 左のy
	ty = -4		; 上端のy
	ly = -6		; 右のy
	nl = -8		; 左の点の添え字(byte単位)
	nr = -10	; 右の点の添え字(byte単位)
	by = -12	; 下端のy

	mov	AX,[BP+npoint]
	dec	AX
	mov	CX,AX			; CX = 本来の npoint
	shl	AX,2
	mov	[BP+npoint],AX		; npoint = (npoint-1)*4

	; 頂上を探す～ ----------------

	LODPTS	SI		; SI = pts
	xor	AX,AX
	mov	[BP+nl],AX	; nl = 0
	mov	[BP+nr],AX	; nr = 0
	MOVPTS	AX,[SI+2]
	mov	[BP+ty],AX	; ty = pts[0].y
	mov	[BP+by],AX	; by = pts[0].y
	MOVPTS	AX,[SI]		; AX = pts[0].x (AX=lx)
	mov	DI,AX		; DI = pts[0].x	(DI=rx)
	mov	BX,4		; BX = 1*4

LOOP1:	; x,y の各最大値と最小値を得るループ
	MOVPTS	DX,[BX+SI+2]	; pts[i].y
	cmp	DX,[BP+ty]
	jge	short S10
		mov	[BP+nl],BX	; nlは、頂上の最初にみつけた点
		mov	[BP+nr],BX	; nrは、頂上の最後に見つけた点
		mov	[BP+ty],DX
		jmp	short S30
		EVEN
S10:
	cmp	[BP+ty],DX
	jne	short S20
		mov	[BP+nr],BX
		jmp	short S30
		EVEN
S20:
	cmp	[BP+by],DX
	jge	short S30
		mov	[BP+by],DX
S30:
	MOVPTS	DX,[BX+SI]	; pts[].x
	cmp	AX,DX
	jl	short S40
		mov	AX,DX
		jmp	short S50
		EVEN

		; へんなところに潜ってるが、クリップの近場にreturn処理が
		; いるのだ～
	RETURN1:
		MRETURN
		EVEN
S40:	; else
		cmp	DI,DX
		jge	short S50
			mov	DI,DX
S50:
	add	BX,4
	loop	short LOOP1
	; ループ終り ----------------

	; 範囲検査
	cmp	ClipXR,AX
	jl	short RETURN1	; 完全に右に外れてる

	cmp	ClipXL,DI
	jg	short RETURN1	; 完全に左に外れてる

	mov	AX,[BP+ty]
	cmp	ClipYB,AX
	jl	short RETURN1	; 完全に下に外れてる

	mov	DX,[BP+by]
	cmp	ClipYT,DX
	jg	short RETURN1	; 完全に上に外れてる

	; **** さあ、ここに来たからには、かならず描画できるんだぞ。 ****
	; (台形による最後のクリップを宛てにしてるから)

	; 描画セグメント設定
	s_mov	ES,graph_VramSeg

	; 下のy座標を先にクリップ枠で切断する
	mov	AX,ClipYB
	cmp	AX,DX
	jge	short S100
		mov	DX,AX
S100:
	mov	[BP+by],DX

	; nlが最初、nr最後ならば、入れ換え。（わかりにくい～）
	; （凸多角形ならば、ある点の他に同じy座標の点は１つ以下にしか
	;　ならない。従って、頂上を共有する点は２つ以内なので、座標列の
	;　端をまたがるのはこのパターンのみとなる）
	mov	AX,[BP+npoint]
	cmp	WORD PTR [BP+nl],0
	jne	short S110
	cmp	[BP+nr],AX
	jne	short S110
		mov	[BP+nl],AX
		mov	WORD PTR [BP+nr],0
S110:

	; 左側辺の、上にはみ出ている部分を飛ばし ***
	mov	BX,[BP+nl]	; BX = nl
	mov	DX,[BP+npoint]
	mov	AX,ClipYT
LOOPL:
	mov	CX,BX
	sub	BX,4
	jns	short SKIPL
	mov	BX,DX
SKIPL:
	CMPPTS	[BX+SI+2],AX
	jl	short LOOPL
	MOVPTS	AX,[BX+SI+2]
	mov	[BP+ly],AX	; ly = pts[nl].y
	mov	[BP+nl],BX

	; 最初の左側辺データを作成

	mov	BX,CX		; BX = i
	MOVPTS	DX,[SI+BX+2]	; DX = ty = pts[i].y
	MOVPTS	DI,[SI+BX]	; DI = wx = pts[i].x
	mov	BX,[BP+nl]
	cmp	DX,ClipYT
	jge	short S200
		mov	AX,DX
		sub	AX,[BP+ly]	; push (ty-ly)
		PUSH	AX
		mov	AX,DI		; AX = ((DI=wx) - pts[nl].x)
		SUBPTS	AX,[SI+BX]
		mov	CX,ClipYT
		sub	CX,DX		; CX = (ClipYT - (DX=ty))
		imul	CX
		POP	CX
		idiv	CX		; DI = (wx-pts[nl].x) * (ClipYT-ty)
		add	DI,AX		;		 / (ty-ly) + wx
		mov	DX,ClipYT	; ty = ClipYT
S200:
	mov	[BP+ty],DX

	xchg	DI,DX
	MOVPTS	AX,[SI+BX]	; pts[nl].x
	mov	CX,[BP+ly]
	sub	CX,DI		; CX = (ly-ty)
	mov	BX,offset trapez_a
	call	make_linework


	; 右側辺の、上にはみ出ている部分を飛ばし ***

	mov	BX,[BP+nr]	; BX = nr
	mov	CX,[BP+npoint]
	mov	AX,ClipYT
LOOPR:
	mov	DI,BX		; DI = i
	add	BX,4		; BX = nr
	cmp	BX,CX
	jle	short SKIPR
	xor	BX,BX
SKIPR:
	CMPPTS	[SI+BX+2],AX
	jl	short LOOPR

	MOVPTS	AX,[SI+BX+2]
	mov	[BP+ry],AX
	mov	[BP+nr],BX

	; 最初の右側辺データを作成
	add	DI,SI
	MOVPTS	CX,[DI+2]	; CX = pts[i].y
	MOVPTS	DI,[DI]		; DI = wx = pts[i].x
	MOVPTS	BX,[SI+BX]	; BX = pts[nr].x

	cmp	CX,[BP+ty]
	jge	short S300
		mov	AX,ClipYT	; (ClipYT - pts[i].y)
		sub	AX,CX
		push	CX
		mov	CX,DI		; (wx - pts[nr].x)
		sub	CX,BX
		imul	CX
		pop	CX
		sub	CX,[BP+ry]	; (pts[i].y - ry)
		idiv	CX
		add	DI,AX		; + wx
S300:
	mov	DX,DI
	mov	AX,BX		; pts[nr].x
	mov	CX,[BP+ry]
	sub	CX,[BP+ty]
	mov	BX,offset trapez_b
	call	make_linework

; 実際に描画していくんだ[よーん] ========================

	mov	SI,[BP+ty]	; SI = ty

DRAWLOOP:
	mov	DI,[BP+ly]	; DI = y2
	cmp	DI,[BP+ry]
	jle	short S400
	mov	DI,[BP+ry]
S400:
	cmp	[BP+by],DI	; 下クリップ
	jle	short S500

	PUSH	DI
	lea	BX,[DI-1]
	sub	BX,SI		; DX = y2-1 - ty
	mov	AX,SI
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,BX
	_mov	ES,graph_VramSeg
	call	vgc_draw_trapezoid	; **** DRAW ****
	POP	SI

	LODPTS	DI		; DI = pts
	cmp	SI,[BP+ly]	; if ( ty == ly )
	jne	short S410

	mov	BX,[BP+nl]
	MOVPTS	DX,[BX+DI]	; DX = pts[nl].x
	sub	BX,4
	jns	short S405
	mov	BX,[BP+npoint]
S405:
	mov	[BP+nl],BX
	MOVPTS	AX,[DI+BX]	; AX = pts[nl].x
	MOVPTS	CX,[DI+BX+2]	; CX = (ly = pts[nl].y) - ty
	mov	[BP+ly],CX
	sub	CX,SI
	mov	BX,offset trapez_a
	; DX = last pts[nl].x
	call	make_linework

S410:				; if ( ty == ry )
	cmp	[BP+ry],SI
	jne	short DRAWLOOP

	mov	BX,[BP+nr]	; BX = nr
	MOVPTS	DX,[BX+DI]
	add	BX,4
	cmp	BX,[BP+npoint]
	jle	short S420
	xor	BX,BX
S420:
	mov	[BP+nr],BX
	MOVPTS	AX,[DI+BX]	; AX = pts[nr].x
	MOVPTS	CX,[DI+BX+2]	; CX = pts[nr].y
	mov	[BP+ry],CX	; ry = CX
	sub	CX,SI		; CX -= SI
	mov	BX,offset trapez_b
	push	offset DRAWLOOP
	jmp	make_linework		; まさか!!
	EVEN

	; 一番下の台形
S500:

	_mov	ES,graph_VramSeg
	mov	BX,[BP+by]
	sub	BX,SI	; DX = by - ty
	mov	AX,SI
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,BX
	call	vgc_draw_trapezoid
	MRETURN
endfunc			; }

END
