; master library - PC98
;
; Description:
;	テキスト画面へのパスカル文字列の書き込み
;	幅指定なし・属性なし
;
; Function/Procedures:
;	void text_putp( unsigned x, unsigned y, char *pasp ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	char * pasp	パスカル文字列の先頭アドレス ( NULLは禁止 )
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
;	92/12/15 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PUTP
	mov	BX,BP	; save BP
	mov	BP,SP

	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+1+DATASIZE)*2
	y	= (RETSIZE+0+DATASIZE)*2
	pasp	= (RETSIZE+0)*2

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

	_push	DS
	_lds	SI,[BP+pasp]

	mov	BP,BX	; restore BP

	mov	CH,0
	mov	CL,[SI]	; 最初のバイトが長さ

	jcxz	short EXIT

	inc	SI

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	DX,9f80h

SLOOP:		lodsb
		xor	AH,AH
		test	AL,0e0h
		jns	short ANK_OR_RIGHT	; 00～7f = ANK
		jp	short ANK_OR_RIGHT	; 80～9f, e0～ff = 漢字
						; (ほんとは 81～9f, e0～fdね)
		cmp	CX,1
		je	short ANK_OR_RIGHT
		dec	CX

		mov	AH,AL
		lodsb				; 2文字目: 40-7e,80-fc
		shl	AH,1
		cmp	AL,DH
		jnb	short SKIP
			cmp	AL,DL
			adc	AX,BX	; (stc)
SKIP:		sbb	AL,BH	; (BH = -2)

		and	AX,7f7fh
		xchg	AH,AL
		stosw
		or	AH,DL
ANK_OR_RIGHT:	stosw
	loop	short SLOOP

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	ret	(2+DATASIZE)*2
endfunc

END
