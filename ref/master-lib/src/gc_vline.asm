PAGE 98,120
; graphics - grcg - vline - PC98V
;
; DESCRIPTION:
;	垂直線の描画(青プレーンのみ)
;
; FUNCTION:
;	void grcg_vline( int x, int y1, int y2 ) ;

; PARAMETERS:
;	int x	x座標
;	int y1	上のy座標
;	int y2	下のy座標
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	・グラフィック画面の青プレーンにのみ描画します。
;	・色をつけるには、グラフィックチャージャーを利用してください。
;	・grc_setclip()によるクリッピングに対応しています。
;	・y1,y2の上下関係は逆でも(速度も)全く変わりなく動作します。
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;
; HISTORY:
;	92/10/5	Initial
;	93/10/15 [M0.21] 縦クリッピングのbugfix (iR氏報告)
;	93/11/26 [M0.22] ↑さらに(;_;) yの小さいほうが負だと…
;	94/ 3/12 [M0.23] 無駄なことやってたので縮小

	.186

	.MODEL SMALL


	.DATA?

		EXTRN	ClipXL:WORD, ClipXR:WORD
		EXTRN	ClipYT:WORD, ClipYH:WORD
		EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

func GRCG_VLINE
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	x  = (RETSIZE+3)*2
	y1 = (RETSIZE+2)*2
	y2 = (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	CX,ClipYH

	mov	BX,[BP+y1]
	mov	DX,[BP+y2]
	cmp	BX,DX
	jl	short GO
	xchg	BX,DX
GO:

	; yの変換とクリップ
	sub	DX,AX
	jl	short RETURN
	sub	BX,AX
	cmp	BH,80h
	sbb	DI,DI
	and	BX,DI

	cmp	BX,CX
	jg	short RETURN
	sub	DX,CX		; DXは負になりえない(Clip枠が正である前提)
	sbb	DI,DI
	and	DX,DI
	add	DX,CX

	mov	AX,[BP+x]	; xのクリップ
	cmp	AX,ClipXL
	jl	short RETURN
	cmp	AX,ClipXR
	jg	short RETURN
	mov	CX,AX
	and	CL,07h
	shr	AX,3
	mov	DI,AX
	mov	AL,80h
	shr	AL,CL

	mov	CX,DX		; 増分の計算
	mov	DX,80-1		; DXが増分(stosbで増える1をあらかじめ減らす)
	sub	CX,BX		; CXは長さ
	imul	BX,BX,80
	add	DI,BX

	mov	ES,ClipYT_seg	; セグメント設定
	inc	CX		; 長さを出す(abs(y2-y1)+1)

	shr	CX,1
	jnb	short S1
	stosb
	add	DI,DX
S1:
	shr	CX,1
	jnb	short S2
	stosb
	add	DI,DX
	stosb
	add	DI,DX
S2:	jcxz	short RETURN
	EVEN
L:	stosb
	add	DI,DX
	stosb
	add	DI,DX
	stosb
	add	DI,DX
	stosb
	add	DI,DX
	loop	short L

RETURN:
	MRETURN
endfunc
END
