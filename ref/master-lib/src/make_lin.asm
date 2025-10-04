PAGE 98,120
; gc_poly library - calc - trapezoid
;
; Subroutines:
;	make_linework
;
; Variables:
;	trapez_a, trapez_b	台形描画作業変数
;
; Description:
;	台形描画用の側辺データ設定
;
; Binding Target:
;	asm routine
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;
;
; Revision History:
;	92/3/21 Initial(koizoid.asm)
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
;	92/7/13 make_line.asmに分離

	.186

	.MODEL SMALL

; LINEWORK構造体の各メンバの定義
; 名称  - ｵﾌｾｯﾄ - 説明
x	= 0	; 現在のx座標
dlx	= 2	; 誤差変数への加算値
s	= 4	; 誤差変数
d	= 6	; 最小横移動量(符号付き)

	.DATA?

	; 台形描画用変数
		EXTRN trapez_a: WORD, trapez_b: WORD

	.CODE

;-------------------------------------------------------------------------
; make_linework - DDA LINE用構造体の作成
; IN:
;	DS:BX : LINEWORK * w ;	書き込み先
;	DX :	int x1 ;	上点のx
;	AX :	int x2 ;	下の点のx
;	CX :	int y2 - y1 ;	上下のyの差
;
; BREAKS:
;	AX,CX,DX,Flags
;
	public make_linework
	EVEN
make_linework	PROC NEAR
	push	SI
	mov	[BX+x],DX	; w.x = x1

	sub	AX,DX		; AX = (x2 - x1)
	cmp	CX,1
	adc	CX,0
	cwd
	idiv	CX
	cmp	DX,8000h
	adc	AX,-1
	mov	[BX+d],AX	; w.d = (x2-x1)/(y2-y1)
	cmp	DX,8000h
	cmc
	sbb	SI,SI
	add	DX,SI
	xor	DX,SI
	xor	AX,AX
	div	CX
	add	AX,SI
	xor	AX,SI
	mov	[BX+dlx],AX	; w.dlx = (x2-x1)%(y2-y1)*0x10000L
	mov	[BX+s],8000h	; w.s = 8000h
	pop	SI
	ret
	EVEN
make_linework	ENDP

END
