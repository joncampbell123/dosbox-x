; master library - polygon - convex - nonclip - varargs
;
; Description:
;	VGA 16color, 凸多角形の描画 高速（機能削減）、可変引数版
;
; Funciton/Procedures:
;	void vgc_polygon_vcx( int npoint, int x1, in y1, ... ) ;
;
; Parameters:
;	int npoint	座標点数
;	int x1,y1...	座標データ
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: V30
;	VGA
;
; Notes:
;	・色指定は、vgc_setcolorを使用してください。
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
;	92/ 7/11 Initial: gc_polcx.asm
;	92/ 7/18 点が少ないときの処理
;	93/ 6/26 Initlal: gc_plvcx.asm/master.lib 0.19
;	94/10/24 Initial: vgcplvcx.asm/master.lib 0.23
;	95/ 3/25 [M0.22k] BUGFIX ClipYT_seg使ったコードになってた

	.186
	.MODEL SMALL
	.DATA?

	EXTRN	graph_VramSeg:WORD	; grp.asm
	EXTRN	graph_VramWidth:WORD	; grp.asm

	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	.CODE
	include func.inc

	EXTRN	make_linework:NEAR
	EXTRN	vgc_draw_trapezoidx:NEAR
	EXTRN	VGC_LINE:callmodel

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret
	EVEN
	endm

;---------------------------------
func _vgc_polygon_vcx	; grcg_polygon_vcx() {
	enter	12,0
	push	SI
	push	DI

	; 引数
	npoint	= (RETSIZE+1)*2
	x1	= (RETSIZE+2)*2

	; ローカル変数
	nl = -10
	nr = -12
	ly = -4
	ry = -2
	ty = -6
	by = -8

	mov	ES,graph_VramSeg
	lea	DI,[BP+x1]

	; 頂点を探す～
	mov	SI,[BP+npoint]	; SI=npoint-1
	dec	SI
	js	short RETURN
	mov	[BP+nr],SI
	mov	DX,SI		; DX:nl
	mov	AX,SI
	shl	AX,2
	add	DI,AX
	mov	BX,SS:[DI+2]	; BX:ty
	mov	CX,BX		; CX:by
	dec	SI
	jg	short LOOP1	; ...LOOP1E

	; 1～2点なら直線
	push	SS:[DI]		; pts[npoint-1].x
	push	BX		; pts[npoint-1].y
	push	[BP+x1]		; pts[0].x
	push	[BP+x1+2]	; pts[0].y
	call	VGC_LINE
RETURN:
	MRETURN

LOOP1:
	sub	DI,4
	mov	AX,SS:[DI+2]
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
	lea	DI,[BP+x1]
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
	mov	CX,SS:[BX+2]
	mov	[BP+ly],CX
	mov	AX,SS:[SI+2]
	mov	[BP+ty],AX
	mov	DX,SS:[SI]
	sub	CX,AX
	mov	AX,SS:[BX]
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
	add	BX,DI
	mov	AX,SS:[BX+2]
	mov	[BP+ry],AX
	mov	CX,BX
	mov	BX,DX
	shl	BX,2
	mov	DX,SS:[BX+DI]
	mov	BX,CX

	; 実際に描画していくんだ[よーん]
L4_010:
	sub	AX,[BP+ty]
	mov	CX,AX
	mov	AX,SS:[BX]
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
	;jg	short HOGE
	jle	short LAST_ZOID		; なんと
HOGE:
	push	SI
	push	DI
	lea	BX,[SI-1]
	xchg	SI,[BP+ty]
	sub	BX,SI
	xchg	AX,SI
	imul	graph_VramWidth
	xchg	SI,AX
	mov	DX,BX
	call	vgc_draw_trapezoidx		; 一番下でない台形
	pop	DI
	pop	SI

	cmp	SI,[BP+ly]
	jne	short L4_040

	; 左側の側辺
	mov	CX,[BP+nl]
	mov	BX,CX
	shl	BX,2
	mov	DX,SS:[BX+DI]	; DX!
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
	mov	CX,SS:[BX+2]
	mov	[BP+ly],CX
	sub	CX,[BP+ty]
	mov	AX,SS:[BX]
	mov	BX,offset trapez_a
	call	make_linework

L4_040:
	mov	AX,[BP+ry]
	cmp	[BP+ty],AX
	jne	short LOOP4

	; 右側の側辺
	mov	SI,[BP+nr]
	mov	BX,SI
	shl	BX,2
	mov	DX,SS:[BX+DI]	; DX!
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L4_050
	xor	SI,SI
L4_050:
	mov	BX,SI
	shl	BX,2
	add	BX,DI
	mov	AX,SS:[BX+2]
	mov	[BP+ry],AX
	jmp	L4_010

LAST_ZOID:
	; 一番下の台形
	mov	SI,[BP+ty]
	mov	BX,[BP+by]
	sub	BX,SI
	mov	AX,SI
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,BX
	call	vgc_draw_trapezoidx
	MRETURN
endfunc		; }

END
