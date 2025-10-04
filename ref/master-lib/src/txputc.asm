; master library - PC98
;
; Description:
;	テキスト画面への文字の書き込み
;	属性なし
;
; Function/Procedures:
;	void text_putc( unsigned x, unsigned y, unsigned chr ) ;
;
; Parameters:
;	unsigned x	左端の座標 ( 0 ～ 79 )
;	unsigned y	上端の座標 ( 0 ～ 24 )
;	unsigned chr	文字(ANK, JIS または SHIFT-JISコード)
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
;	正しくない漢字コードが与えられた場合、おかしな文字が表示されます。
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


func TEXT_PUTC
	mov	DX,BP	; push BP
	mov	BP,SP

	mov	CX,DI	; push DI

	; 引数
	x	= (RETSIZE+2)*2
	y	= (RETSIZE+1)*2
	chr	= (RETSIZE+0)*2

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

	mov	AX,[BP + chr]

	mov	BP,DX	; pop BP

	or	AH,AH	; 半角
	jz	short ANK_OR_RIGHT

	cmp	AH,80h	; 		81-9f e0-fd ?
	jb	short JIS
KANJI:		; SHIFT-JIS to JIS
		shl	AH,1
		cmp	AL,9fh
		jnb	short SKIP
			cmp	AL,80h
			adc	AX,0fedfh
SKIP:		sbb	AX,0dffeh		; -(20h+1),-2
		and	AX,07f7fh

JIS:		xchg	AH,AL
		sub	AL,20h
		stosw
		or	AH,80h
ANK_OR_RIGHT:
	stosw

	mov	DI,CX	; pop	DI
	ret	6
endfunc

END
