; master library - PC98
;
; Description:
;	テキスト画面へのパスカル文字列の書き込み
;	幅指定あり・属性なし
;
; Function/Procedures:
;	void text_putnp( unsigned x, unsigned y, char *pasp, unsigned wid ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	char * pasp	パスカル文字列の先頭アドレス ( NULLは禁止 )
;	unsigned wid	フィールド幅
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
;	最後の文字が漢字１バイト目の場合、半角空白になります。
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

func TEXT_PUTNP
	push	BP
	mov	BP,SP

	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	pasp	= (RETSIZE+2)*2
	wid	= (RETSIZE+1)*2

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

	mov	BP,[BP+wid]	; フィールド幅

	mov	CH,0
	mov	CL,[SI]	; 最初のバイトが長さ

	min2	CX,BP,DX
	jcxz	short EXIT
	sub	BP,CX		; BP = 残りの(空白を埋める)幅

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
		je	short FILLSPACE
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

FILLSPACE:
	add	CX,BP
	mov	AX,' '	; SPACE
	rep	stosw

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
