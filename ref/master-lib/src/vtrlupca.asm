; master library - vtext
;
; Description:
;	テキスト画面の上スクロール
;	属性あり
;
; Function/Procedures:
;	void vtext_roll_up_ca( int fillchar, int fillatr ) ;
;
; Parameters:
;	fillchar	新しく現れた部分を埋める文字
;	fillatr		〃属性
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
; Revision History:
;	93/10/10 Initial
;	93/11/ 7 VTextState 導入
;	94/ 4/ 9 Initial: vtrlupca.asm/master.lib 0.23
;	95/ 2/ 3 [M0.23] 必要な部分のみ更新するようにした

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN text_RollTopOff : WORD
	EXTRN text_RollHeight : WORD
	EXTRN text_RollWidth : WORD
	EXTRN TextVramWidth : WORD
	EXTRN TextVramSize : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_ROLL_UP_CA	; vtext_roll_up_ca() {
	push	BP
	mov	BP,SP
	; 引数
	fillchar = (RETSIZE+2)*2
	fillatr = (RETSIZE+1)*2
	push	SI
	push	DI

	les	DI,TextVramAdr
	add	DI,text_RollTopOff

	mov	DX,text_RollHeight
	mov	AX,text_RollWidth
	mov	CX,TextVramWidth

	mov	BX,CX		; CX = BX
	shl	BX,1		; BX = Width * 2
	lea	SI,[DI+BX]	; SI = DI + Width * 2
	mov	BX,CX		;
	sub	BX,AX		; BX = Width - RollWidth
	shl	BX,1		; BX =(Width - RollWidth)*2

LOOPY:
	mov	CX,AX
	rep	movs word ptr es:[DI],word ptr es:[SI]
	lea	SI,[SI+BX]
	lea	DI,[DI+BX]
	dec	DX
	jg	short LOOPY

	mov	CX,AX
	mov	AL,[BP+fillchar]
	mov	AH,[BP+fillatr]
	rep	stosw

	; 描画
	test	VTextState,1
	jne	short NO_REF

	mov	DI,word ptr TextVramAdr
	add	DI,text_RollTopOff
	mov	DX,text_RollHeight
	inc	DX
	mov	CX,text_RollWidth
	mov	BX,TextVramWidth
	add	BX,BX
REFRESH_LOOP:
	mov	AH,0ffh
	int	10h
	add	DI,BX
	dec	DX
	jge	short REFRESH_LOOP
NO_REF:

	pop	DI
	pop	SI
	pop	BP
	ret	4
endfunc			; }

END
