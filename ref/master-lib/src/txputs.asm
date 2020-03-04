; master library - PC98
;
; Description:
;	テキスト画面への文字列の書き込み
;	幅指定なし・属性なし
;
; Function/Procedures:
;	void text_puts( unsigned x, unsigned y, char *strp ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	char * strp	文字列の先頭アドレス ( NULLは禁止 )
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
;	
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

func TEXT_PUTS
	mov	DX,BP	; push BP
	mov	BP,SP

	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+1+DATASIZE)*2
	y	= (RETSIZE+0+DATASIZE)*2
	strp	= (RETSIZE+0)*2

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
	_lds	SI,[BP+strp]

	mov	BP,DX	; pop BP

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	CX,7f7fh
	mov	DX,9f80h

	lodsb
	or	AL,AL
	je	short EXITLOOP

	EVEN
SLOOP:		xor	AH,AH
		cmp	AL,DL	; 80h		81-9f e0-fd ?
		jbe	short ANK_OR_RIGHT
		cmp	AL,DH	; 9fh
		jbe	short KANJI
		cmp	AL,BL	; 0dfh
		jbe	short ANK_OR_RIGHT
	;	cmp	AL,0fdh
	;	jnb	short ANK_OR_RIGHT
KANJI:			mov	AH,AL
			lodsb	; 2文字目: 40-7e,80-fc
			shl	AH,1	; e0..fc->60..98->40..78 または
					; 81..9f->22..5e->02..3e にする

						; 9f-fc -> 21-7e
			cmp	AL,DH		; 40-7e -> 21-5f,--ah
			jnb	short SKIP	; 80-9e -> 60-7e,--ah
				cmp	AL,DL
				adc	AX,BX	; (stc)
SKIP:			sbb	AL,BH	; 0feh

			and	AX,CX
			xchg	AH,AL
			stosw
			or	AH,DL
ANK_OR_RIGHT:	stosw

		lodsb
		or	AL,AL
	jne	short SLOOP
EXITLOOP:

	_pop	DS
	pop	DI
	pop	SI
	ret	(2+datasize)*2
endfunc

END
