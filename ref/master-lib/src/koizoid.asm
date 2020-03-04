PAGE 98,120
; gc_poly library - grcg - trapezoid - PC98V
;
; Subroutines:
;	draw_trapezoid
;
; Variables:
;	trapez_a, trapez_b	台形描画作業変数
;
; Description:
;	台形塗りつぶし(grc_setclip()で設定した領域が対象)
;
; Binding Target:
;	asm routine
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
;	・grc_setclip()によるクリッピングに対応しています。
;	　ｙ座標が上境界にかかっていると遅くなります。
;	　（上境界との交点を計算せずに、上から順に調べているため）
;	・側辺同士が交差している場合、ねじれた台形（２つの三角形が頂点で
;	　接している状態）を描画します。
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
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
;	92/6/4	grcg_trapezoid()関数をgc_zoid.asmに分割。
;	92/6/5	bugfix(make_lineworkのCXが0の時のdiv 0回避)
;	92/6/5	下端クリップ対応
;	92/6/12 加速～
;	92/6/16 TASMに対応
;	92/7/13 make_lin.asmを分離
;	93/ 5/29 [M0.18] .CONST->.DATA
;	94/ 2/23 [M0.23] make_lineworkのEXTRNを削除(それだけ(^^;)

	.186

	.MODEL SMALL

; LINEWORK構造体の各メンバの定義
; 名称  - ｵﾌｾｯﾄ - 説明
x	= 0	; 現在のx座標
dlx	= 2	; 誤差変数への加算値
s	= 4	; 誤差変数
d	= 6	; 最小横移動量(符号付き)

	.DATA

	EXTRN	EDGES:WORD

	.DATA?

	EXTRN	ClipXL:WORD, ClipXW:WORD, ClipYB_adr:WORD

	; 台形描画用変数
	EXTRN trapez_a: WORD, trapez_b: WORD

	.CODE

;-------------------------------------------------------------------------
; draw_trapezoid - 台形の描画
; IN:
;	SI : unsigned yadr	; 描画する三角形の上のラインの左端のVRAMｵﾌｾｯﾄ
;	DX : unsigned ylen	; 描画する三角形の上下のライン差(y2-y1)
;	ES : unsigned gseg	; 描画するVRAMのセグメント(ClipYT_seg)
;	trapez_a		; 側辺aのxの初期値と傾き
;	trapez_b		; 側辺bのxの初期値と傾き
;	ClipYB_adr		; 描画するVRAMの最下座標の左端のアドレス
;	ClipXL			; 描画するVRAMの左側切り落とし x 座標
;	ClipXW			; ClipXLと、右側 x 座標の差(正数)
; BREAKS:
;	AX,BX,CX,DX,SI,DI,flags
;
	public draw_trapezoid
draw_trapezoid	PROC NEAR
	mov	AX,ClipYB_adr
	mov	CS:[yb_adr],AX
	mov	AX,[trapez_a+d]
	mov	CS:[trapez_a_d],AX
	mov	AX,[trapez_b+d]
	mov	CS:[trapez_b_d],AX

	mov	AX,[trapez_a+dlx]
	mov	CS:[trapez_a_dlx],AX
	mov	AX,[trapez_b+dlx]
	mov	CS:[trapez_b_dlx],AX

	mov	AX,ClipXL
	mov	CS:[clipxl_1],AX
	mov	CS:[clipxl_2],AX
	mov	AX,ClipXW
	mov	CS:[clipxw_1],AX

	jmp	short $+2

	push	BP

	mov	CX,[trapez_a+x]
	mov	BP,[trapez_b+x]

YLOOP:
	; 水平線 (with clipping) start ===================================
	; IN: SI... x=0の時のVRAM ADDR(y*80)	BP,CX... 二つのx座標
	cmp	SI,1234h
	org $-2
yb_adr dw ?
	ja	short SKIP_HLINE ; yが範囲外 → skip

	mov	AX,1234h	; クリップ枠左端分ずらす
	org $-2
clipxl_1 dw ?			; ClipXL
	sub	CX,AX
	mov	BX,BP
	sub	BX,AX

	test	CX,BX		; xが共にマイナスなら範囲外
	js	short SKIP_HLINE

	cmp	CX,BX
	jg	short S10
	xchg	CX,BX		; CXは必ず非負になる
S10:
	cmp	BH,80h		; if BX < 0 then BX := 0
	sbb	AX,AX
	and	BX,AX

	mov	DI,1234h	; ClipXW
	org $-2
clipxw_1 dw ?
	sub	CX,DI		; if CX >= ClipXW then   CX := ClipXW
	sbb	AX,AX
	and	CX,AX
	add	CX,DI

	sub	CX,BX		; CX := bitlen
				; BX := left-x
	jl	short SKIP_HLINE ; ともに右に範囲外 → skip

	add	BX,1234h	; ClipXL
	org $-2
clipxl_2 dw ?
	mov	DI,BX		; addr := yaddr + xl div $10 * 2
	shr	DI,4
	shl	DI,1
	add	DI,SI

	and	BX,0Fh		; BX := xl and $0F
	add	CX,BX
	sub	CX,16
	shl	BX,1
	mov	AX,[EDGES+BX]	; 左エッジ
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short LASTW
	stosw
	mov	AX,0FFFFh
	rep stosw
LASTW:	and	AX,[EDGES+2+BX]	; 右エッジ
	stosw
	; 水平線 (with clipping) end ===================================

SKIP_HLINE:
	add	[trapez_a+s],1234h
	org $-2
trapez_a_dlx	dw ?
	mov	CX,[trapez_a+x]
	adc	CX,1234h
	org $-2
trapez_a_d	dw ?
	mov	[trapez_a+x],CX

	add	[trapez_b+s],1234h
	org $-2
trapez_b_dlx	dw ?
	adc	BP,1234h
	org $-2
trapez_b_d	dw ?

	add	SI,80			; yadr
	dec	DX			; ylen
	js	short OWARI
	jmp	YLOOP		; うぐぐ(;_;)
OWARI:
	mov	[trapez_b+x],BP
	pop	BP
	ret
	EVEN
draw_trapezoid	ENDP
END
