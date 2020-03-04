; master library - text - puts - vertical
;
; Description:
;	テキスト画面への縦書き文字列表示(属性つき)
;
; Function/Procedures:
;	void text_vputsa(unsigned x, unsigned y, const char *str, unsigned atrb);
;
; Parameters:
;	x, y  開始テキスト座標
;	str   文字列
;	atrb  属性
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
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi
;
; Revision History:
;	94/ 1/19 Initial: txputsa.asm/master.lib 0.22

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_VPUTSA
	mov	DX,BP	; push BP
	mov	BP,SP

	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+2+DATASIZE)*2
	y	= (RETSIZE+1+DATASIZE)*2
	strp	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	mov	AX,[BP + y]	; アドレス計算
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,[BP + x]
	shl	DI,1

	mov	CX,[BP + atrb]

	_push	DS
	_lds	SI,[BP+strp]

	mov	BP,DX	; pop BP

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	DX,9f80h

	lodsb
	or	AL,AL
	jz	short EXITLOOP
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
			lodsb		; 2文字目: 40-7e,80-fc
			shl	AH,1	; e0..fc->60..98->40..78 または
					; 81..9f->22..5e->02..3e にする

						; 9f-fc -> 21-7e
			cmp	AL,DH		; 40-7e -> 21-5f,--ah
			jnb	short SKIP	; 80-9e -> 60-7e,--ah
				cmp	AL,DL
				adc	AX,BX	; (stc)
SKIP:			sbb	AL,BH	; 0feh

			and	AX,7f7fh
			xchg	AH,AL
			mov	ES:[DI + 2000h],cx
			stosw
			or	AL,DL
ANK_OR_RIGHT:
		mov	ES:[DI + 2000h],cx
		stosw

		add	DI,79*2	;次の行へ
		or	AH,AH
		jz	short ANK_ROW
		sub	DI,2
ANK_ROW:
		lodsb
		or	AL,AL
	jne	short SLOOP
EXITLOOP:

	_pop	DS

	pop	DI
	pop	SI
	ret	(3+datasize)*2
endfunc

END
