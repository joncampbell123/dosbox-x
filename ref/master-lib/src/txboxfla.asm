; master library - PC98
;
; Description:
;	テキスト画面の長方形範囲を指定属性で埋める
;
; Function/Procedures:
;	void text_boxfilla( unsigned x1, unsigned y1, unsigned x2, unsigned y2,
;				unsigned atrb ) ;
;
; Parameters:
;	unsigned x1	左端の横座標( 0 ～ 79 )
;	unsigned y1	上端の縦座標( 0 ～ 24 )
;	unsigned x2	右端の横座標( 0 ～ 79 )
;	unsigned y2	下端の縦座標( 0 ～ 24 )
;	unsigned atrb	属性
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	x1 <= x2 かつ y1 <= y2でなければならない。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/15 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_BOXFILLA
	push	DI
	pushf
	CLI
	add	SP,(2+retsize)*2
			; パラメータ領域の先頭( flags,DI,retadr )

	pop	AX	; color
	pop	DX	; y2
	pop	BX	; x2
	pop	CX	; Y1

	mov	DI,CX
	shl	DI,1
	shl	DI,1
	add	DI,CX
	shl	DI,1
	add	DI,TextVramSeg
	add	DI,200h
	mov	ES,DI	; これで ESに (0,y1)のテキスト属性セグメントが入った

	pop	DI	; X1

	sub	SP,(5+2+retsize)*2
			; (5(ﾊﾟﾗﾒｰﾀの数)+1(ﾘﾀｰﾝｱﾄﾞﾚｽ)+2(DI,flags))*2
	popf

	sub	BX,DI	; BX = X2 - X1 + 1
	inc	BX	;

	sub	DX,CX
	mov	CX,DX
	shl	DX,1
	shl	DX,1
	add	DX,CX
	shl	DX,1
	shl	DX,1
	shl	DX,1
	shl	DX,1
	add	DI,DX
	shl	DI,1	; これで DI には(x1,y2)のoffsetが入った。

	lea	DX,[BX+80]
	shl	DX,1	; 毎回の減少分

	EVEN
SLOOP:		mov     CX,BX
		rep stosw	; 一行分の書き込み
		sub     DI,DX	; 下の行に移動
	jnb	short SLOOP

	pop	DI
	ret	10
endfunc

END
