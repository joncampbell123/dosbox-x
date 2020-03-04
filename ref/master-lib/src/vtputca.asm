; master library - vtext
;
; Description:
;	テキスト画面への文字の書き込み
;	属性つき
;
; Function/Procedures:
;	void vtext_putca( unsigned x, unsigned y, unsigned chr, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	左端の座標
;	unsigned y	上端の座標
;	unsigned chr	文字(ANK, JIS, SHIFT-JISコード)
;	unsigned atrb	属性
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
;	94/ 4/ 9 Initial: vtxputca.asm/master.lib 0.23
;	94/ 4/13 [M0.23] JISコード対応

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

func VTEXT_PUTCA	; vtext_putca() {
	mov	CX,BP
	mov	BP,SP
	push	DI

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	chr	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	les	DI,TextVramAdr
	mov	AX,[BP + y]	; アドレス計算
	mul	TextVramWidth
	add	AX,[BP + x]
	shl	AX,1
	add	DI,AX

	mov	AX,[BP + chr]	; AL = chr(L)
	cmp	AH,0
	jle	short NO_JIS

	; JIS → SHIFT JIS
	add	AX,0ff1fh
	shr	AH,1
	jnc	short skip0
	add	AL,5eh
skip0:
	cmp	AL,7fh
	sbb	AL,-1
	cmp	AH,2eh
	jbe	short skip1
	add	AH,40h
skip1:
	add	AH,71h

NO_JIS:

	mov	DX,[BP + atrb]
	xchg	DL,AH		; DL = chr(H), AH = atrb
	mov	BP,CX

	test	byte ptr VTextState,01
	jnz	short DIRECT

	mov	CX,1
	test	DL,DL
	jz	short ANKTEST
	inc	CX
	cmp	ES:[DI+2],AX
	je	short	ANK2
	mov	ES:[DI+2],AX
	mov	AL,DL
	jmp	short WRITE
ANK2:
	mov	AL,DL
ANKTEST:
	cmp	ES:[DI],AX
	je	short NO_REF
WRITE:
	mov	ES:[DI],AX
	mov	AH,0ffh
	int	10h
NO_REF:
	pop	DI
	ret	8
	EVEN

DIRECT:
	test	DL,DL
	jz	short DANK
	mov	ES:[DI+2],AX
	mov	AL,DL
DANK:
	mov	ES:[DI],AX
	pop	DI
	ret	8
endfunc			; }

END
