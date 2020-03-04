; master library - PC98
;
; Description:
;	テキスト画面へのパスカル文字列の書き込み
;	幅指定なし・属性あり
;
; Function/Procedures:
;	void text_putp( unsigned x, unsigned y, char *pasp, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	char * pasp	パスカル文字列の先頭アドレス ( NULLは禁止 )
;	unsigned atrb	文字属性
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
;	最後の文字が漢字１バイト目の場合、そのままのコードを書き込みます。
;	漢字の判定は厳密ではありません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/18 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PUTPA
	push	BP
	mov	BP,SP

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	pasp	= (RETSIZE+2)*2
	atrb	= (RETSIZE+1)*2

	push	SI
	push	DI
	_push	DS

	mov	AX,[BP + y]	; アドレス計算
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1		; DI = y * 10
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,[BP + x]
	shl	DI,1

	_lds	SI,[BP+pasp]	; 文字列
	mov	CH,0
	mov	CL,[SI]		; 最初のバイトが長さ
	jcxz	short EXIT
	inc	SI

	mov	DX,CX		; 長さを保存

	mov	BP,[BP+atrb]

	mov	BX,0fedfh ; -2,-(20h + 1)

SLOOP:		lodsb
		xor	AH,AH
		test	AL,0e0h
		jns	short ANK_OR_RIGHT	; 00～7f = ANK
		jpe	short ANK_OR_RIGHT	; 80～9f, e0～ff = 漢字
						; (ほんとは 81～9f, e0～fdね)
		cmp	CX,1
		je	short ANK_OR_RIGHT
		dec	CX

		mov	AH,AL
		lodsb				; 2文字目: 40-7e,80-fc
		shl	AH,1
		cmp	AL,9fh
		jnb	short SKIP
			cmp	AL,80h
			adc	AX,BX	; (stc)
SKIP:		sbb	AL,BH	; (BH = -2)

		and	AX,7f7fh
		xchg	AH,AL
		stosw
		or	AH,80h
ANK_OR_RIGHT:	stosw
	loop	short SLOOP

	; 文字属性を書き込む
	mov	AX,BP
	mov	CX,DX
	shl	DX,1
	sub	DI,DX
	add	DI,2000h	; 属性アドレス計算
	rep	stosw

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
