; master library - VGA 16color - trapezoid - noclip
;
; Description:
;	台形塗りつぶし(ES:～が対象)
;
; Subroutines:
;	vgc_draw_trapezoidx
;
; Variables:
;	trapez_a, trapez_b	台形描画作業変数
;
; Binding Target:
;	asm routine
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	・あらかじめ色や演算モードを vgc_setcolor()で指定してください。
;	・クリッピングは一切しません。
;	・側辺同士が交差していると、交差した描画をします。
;
; Author:
;	恋塚昭彦
;
; 関連関数:
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
;	92/7/10 クリッピング無し＆側辺交差無し版(koizoidx.asm)
;	92/7/14 加速
;	92/7/17 最初の点が同じxだと変になってたbugをfix...減速(;_;)
;		今度は、横幅 1dotの並行な図形はすごく遅いぞ
;	93/3/2 あう(>_<)<-意味なし
;	93/ 5/29 [M0.18] .CONST->.DATA
;	94/ 2/23 [M0.23] make_lineworkのEXTRNを削除(それだけ(^^;)
;	94/ 4/ 9 Initial: vgczoidx.asm/master.lib 0.23

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

	; 台形描画用変数
	EXTRN trapez_a: WORD, trapez_b: WORD
	EXTRN graph_VramWidth: WORD

	.CODE
	include vgc.inc

;-------------------------------------------------------------------------
; draw_trapezoidx - 台形の描画 高速（機能削減）版
; IN:
;	SI : unsigned yadr	; 描画する三角形の上のラインの左端のVRAMｵﾌｾｯﾄ
;	DX : unsigned ylen	; 描画する三角形の上下のライン差(y2-y1)
;	ES : unsigned gseg	; 描画するVRAMのセグメント(ClipYT_seg)
;	trapez_a		; 側辺aのxの初期値と傾き
;	trapez_b		; 側辺bのxの初期値と傾き
; BREAKS:
;	AX,BX,CX,DX,SI,DI,flags
;
	public vgc_draw_trapezoidx
vgc_draw_trapezoidx	PROC NEAR
	push	BP
	mov	AX,graph_VramWidth
	mov	CS:add_width,AX

	mov	AX,[trapez_a+d]
	mov	CS:trapez_a_d,AX
	mov	AX,[trapez_b+d]
	mov	CS:trapez_b_d,AX

	mov	AX,[trapez_a+dlx]
	mov	CS:trapez_a_dlx,AX
	mov	AX,[trapez_b+dlx]
	mov	CS:trapez_b_dlx,AX

	jmp	$+2		; break pipeline

	mov	BP,[trapez_a+x]
	mov	CX,[trapez_b+x]

	EVEN
YLOOP:
	; 水平線 (without clipping) start ===================================
	; IN: SI... x=0の時のVRAM ADDR(y*80)	BP,CX... 二つのx座標
	mov	BX,BP

	sub	CX,BX		; CX := bitlen
				; BX := left-x
	sbb	AX,AX
	xor	CX,AX
	sub	CX,AX
	and	AX,CX
	sub	BX,AX

	mov	DI,BX		; addr := yaddr + xl div 8
	shr	DI,3
	add	DI,SI

	and	BX,7		; BX := xl and 7
	add	CX,BX
	shl	BX,1
	mov	AL,byte ptr [EDGES+BX]	; 左エッジ
	not	AX

	mov	BX,CX		;
	and	BX,7
	shl	BX,1

	shr	CX,3
	jz	short LASTW

	test	ES:[DI],AL
	stosb

	mov	AL,0ffh
	dec	CX
	jz	short REPSTOSB2
REPSTOSB:
	or	ES:[DI],AL
	inc	DI
	loop	short REPSTOSB
REPSTOSB2:
LASTW:	and	AL,byte ptr [EDGES+2+BX]	; 右エッジ
	test	ES:[DI],AL
	mov	ES:[DI],AL
	; 水平線 (without clipping) end ===================================

	add	[trapez_b+s],1234h
	org $-2
trapez_b_dlx	dw ?
	mov	CX,[trapez_b+x]
	adc	CX,1234h
	org $-2
trapez_b_d	dw ?
	mov	[trapez_b+x],CX

	add	[trapez_a+s],1234h
	org $-2
trapez_a_dlx	dw ?
	adc	BP,1234h
	org $-2
trapez_a_d	dw ?

	add	SI,1234h		; yadr
	org $-2
add_width	dw ?
	dec	DX			; ylen
	jns	short YLOOP

	mov	[trapez_a+x],BP
	pop	BP
	ret
vgc_draw_trapezoidx	ENDP

END
