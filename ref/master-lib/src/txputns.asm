; master library - PC98
;
; Description:
;	テキスト画面への文字列の書き込み
;	幅指定・属性なし
;
; Function/Procedures:
;	void text_putns( unsigned x, unsigned y, char *strp, unsigned wid ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	char * strp	文字列の先頭アドレス ( NULLは禁止 )
;	unsigned wid	表示領域の桁数 ( 0 ならば何もしない )
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
;	漢字が最後の桁に半分またがる場合、半角空白に置き代わります。
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

func TEXT_PUTNS
	mov	BX,BP	; save BP
	mov	BP,SP

	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+2+DATASIZE)*2
	y	= (RETSIZE+1+DATASIZE)*2
	strp	= (RETSIZE+1)*2
	wid	= (RETSIZE+0)*2

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

	mov	CX,[BP + wid]

	mov	BP,BX	; restore BP

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	DX,9f80h

	jcxz	short EXIT
	jmp	short SLOOP

POST_KANJI:	; 最後の漢字の後に一つ空白を入れる
		inc	CX
FILL_SPACE:	; 残り桁に空白を詰める
		mov	AX,' '	; space
		rep stosw
		jmp	short EXIT
	EVEN

SLOOP:		lodsb
		or	AL,AL
		jz	short FILL_SPACE
		xor	AH,AH
		cmp	AL,DL	; 80h		81-9f e0-fd ?
		jbe	short ANK_OR_RIGHT
		cmp	AL,DH	; 9fh
		jbe	short KANJI
		cmp	AL,BL	; 0dfh
		jbe	short ANK_OR_RIGHT
	;	cmp	AL,0fdh
	;	jnb	short ANK_OR_RIGHT
KANJI:			dec	CX
			je	short POST_KANJI

			mov	AH,AL
			lodsb	; 2文字目: 40-7e,80-fc
			shl	AH,1
			cmp	AL,DH
			jnb	short SKIP
				cmp	AL,DL
				adc	AX,BX	; (stc)
SKIP:			sbb	AL,BH	; (BH = -2)

			and	AX,7f7fh
			xchg	AH,AL
			stosw
			or	AH,DL
ANK_OR_RIGHT:	stosw
	loop	SLOOP

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	ret	(3+datasize)*2
endfunc

END
