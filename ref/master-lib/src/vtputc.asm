; master library - vtext
;
; Description:
;	テキスト画面への文字の書き込み
;	属性なし
;
; Function/Procedures:
;	void vtext_putc( unsigned x, unsigned y, unsigned chr ) ;
;
; Parameters:
;	unsigned x	左端の座標
;	unsigned y	上端の座標
;	unsigned chr	文字(ANK, JIS, SHIFT-JISコード)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
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
;	のろ/V
;
; Revision History:
;	93/10/11 Initial
;	93/11/ 7 VTextState 導入
;	94/ 4/ 9 Initial: vtxputc.asm/master.lib 0.23
;	94/ 4/13 [M0.23] JISコード対応
;	94/12/21 [M0.23] JIS->SJIS変換処理の置き換え

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramWidth:WORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

func VTEXT_PUTC	; vtext_putc() {
	mov	BX,SP
	push	DI

	; 引数
	x	= (RETSIZE+2)*2
	y	= (RETSIZE+1)*2
	chr	= (RETSIZE+0)*2

	les	DI,TextVramAdr
	mov	AX,SS:[BX+y]	; アドレス計算
	mul	TextVramWidth
	add	AX,SS:[BX+x]
	shl	AX,1
	add	DI,AX

	mov	CX,1

	mov	AX,SS:[BX+chr]	; AL = chr(L)
	cmp	AH,0
	jle	short NOT_JIS

	; JIS → SHIFT JIS
	sub	AX,0de82h
	rcr	AH,1
	jb	short SKIP0
		cmp	AL,0deh
		sbb	AL,05eh
SKIP0:	xor	AH,20h

NOT_JIS:
	test	byte ptr VTextState,01
	jnz	short DIRECT

	mov	CX,1
	test	AH,AH
	jz	short ANKTEST
	inc	CX
	cmp	ES:[DI+2],AL
	je	short	ANK2
	mov	ES:[DI+2],AL
	mov	AL,AH
	jmp	short WRITE
ANK2:
	mov	AL,AH
ANKTEST:
	cmp	ES:[DI],AL
	je	short NO_REF
WRITE:
	mov	ES:[DI],AL
	mov	AH,0ffh
	int	10h
NO_REF:
	pop	DI
	ret	6
	EVEN

DIRECT:
	test	AH,AH
	jz	short DANK
	mov	ES:[DI+2],AL
	mov	AL,AH
DANK:
	mov	ES:[DI],AL
	pop	DI
	ret	6
endfunc		; }

END
