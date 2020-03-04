PAGE 98,120
; koichan library - graphics - grcg - trapezoid - PC98V
;
; Function:
;	void far _pascal grcg_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 ) ;
;
; Description:
;	台形塗りつぶし(青プレーンのみ)
;
; Parameters:
;	int y1			１つ目の水平線のｙ座標
;	int x11, x12		１つ目の水平線の両端のｘ座標
;	int y2			２つ目の水平線のｙ座標
;	int x21, x22		２つ目の水平線の両端のｘ座標
;
; Binding Target:
;	Microsoft-C / Turbo-C ( Small Model )
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・クリッピングを行っています。
;	　左右方向は、grc_setclip()によるクリッピングに対応していますが、
;	　上下方向は、画面枠でクリップします。呼び出し側で対応して(ﾋｰ)
;	　ｙ座標が上境界にかかっていると遅くなります。
;	　（上境界との交点を計算せずに、上から順に調べているため）
;	・位置関係は上下、左右とも自由ですが、ねじれている場合
;	　ねじれた台形（２つの三角形が頂点で接している状態）を描画します。
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/3/21 Initial
;	92/3/29 bug fix, 台形に側辺の交差を認めるようにした
;	92/4/2 少々加速
;	92/4/18 三角形ﾙｰﾁﾝから分離。ｸﾘｯﾋﾟﾝｸﾞ付加。
;	92/4/19 少々加速
;	92/5/7 自己書き換えにより加速
;	92/5/20 任意ｸﾘｯﾋﾟﾝｸﾞの横にだけ対応。:-)
;	92/5/22 ↑これも自己書き換え～
;	92/6/6	bug fix
;	92/6/13 bug fix
;	92/6/16 TASM対応
;	95/ 3/25 [M0.22k] BUGFIX ClipYT=0を仮定していた

	.186

	.MODEL SMALL

	.DATA?
		EXTRN trapez_a:WORD, trapez_b:WORD	; 台形描画用変数
		EXTRN ClipYT_seg:WORD			; VRAM上端セグメント
		EXTRN ClipYT:WORD			; ｽｸﾘｰﾝの上下端のy差
		EXTRN ClipYH:WORD			; ｽｸﾘｰﾝの上下端のy差

	.CODE
		include func.inc
		EXTRN make_linework:NEAR, draw_trapezoid:NEAR

;=========================================================================
; void _pascal grcg_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 )

FUNC GRCG_TRAPEZOID
	push	BP
	mov	BP,SP
	push	DI
	push	SI
	mov	ES,ClipYT_seg
	mov	AX,ClipYT
	mov	CX,ClipYH

	; 引数
	y1  = (RETSIZE+6)*2
	x11 = (RETSIZE+5)*2
	x12 = (RETSIZE+4)*2
	y2  = (RETSIZE+3)*2
	x21 = (RETSIZE+2)*2
	x22 = (RETSIZE+1)*2

	mov	SI,[BP+y1]
	sub	SI,AX
	js	short Lno_draw
	mov	DI,[BP+y2]
	sub	DI,AX
	js	short Lno_draw

	cmp	SI,DI
	jg	short Lgyaku

	or	DI,DI
	js	short Lno_draw
	cmp	SI,CX
	jg	short Lno_draw
	sub	DI,CX			; if ( y2 >= Ymax )
	sbb	AX,AX			;	y2 = Ymax ;
	and	DI,AX
	add	DI,CX
	sub	DI,SI			; DI = y2 - y1
	jl	short Lno_draw

	mov	AX,[BP+x21]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_a	;a
	mov	DX,[BP+x11]
	call	make_linework

	mov	AX,[BP+x22]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_b	;b
	mov	DX,[BP+x12]
	push	offset Ldraw
	jmp	make_linework

Lgyaku:
	xchg	SI,DI			; y2<->y1

	or	DI,DI
	js	short Lno_draw
	cmp	SI,CX
	jg	short Lno_draw
	sub	DI,CX			; if ( y2 >= Ymax )
	sbb	AX,AX			;	y2 = Ymax ;
	and	DI,AX
	add	DI,CX

	sub	DI,SI			; DI = y2 - y1
	jl	short Lno_draw

	mov	AX,[BP+x11]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_a	;a
	mov	DX,[BP+x21]
	call	make_linework

	mov	AX,[BP+x12]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_b	;b
	mov	DX,[BP+x22]
	call	make_linework
Ldraw:
	imul	SI,SI,80
	mov	DX,DI
	call	draw_trapezoid
Lno_draw:
	pop	SI
	pop	DI
	leave
	ret	12
ENDFUNC

END
