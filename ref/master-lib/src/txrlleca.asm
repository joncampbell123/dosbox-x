; master library - text - scroll - left - character - attribute
;
; Description:
;	テキスト画面の左スクロール
;	属性あり
;
; Function/Procedures:
;	void text_roll_left_ca(unsigned fillchar, unsigned filatr);
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
;	94/ 3/30 Initial: txrlleca.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD

	.CODE

func TEXT_ROLL_LEFT_CA	; text_roll_left_ca() {
	push	BP
	mov	BP,SP
	; 引数
	fillchar = (RETSIZE+2)*2
	fillatr = (RETSIZE+1)*2
	push	SI
	push	DI

	mov	DI,text_RollTopOff
	mov	DX,text_RollHeight
	inc	DX
	lea	SI,[DI+2]
	mov	AX,text_RollWidth
	dec	AX
	mov	ES,TextVramSeg
	mov	BX,80
	sub	BX,AX
	shl	BX,1
LOOPY:
	mov	CX,AX
	rep	movs word ptr ES:[DI],word ptr ES:[SI]
	lea	SI,[SI+BX+2000h-160]
	lea	DI,[DI+BX+2000h-160]
	mov	CX,AX
	rep	movs word ptr ES:[DI],word ptr ES:[SI]
	lea	SI,[SI+BX-2000h]
	lea	DI,[DI+BX-2000h]
	dec	DX
	jg	short LOOPY

	shl	AX,1
	mov	DI,text_RollTopOff
	add	DI,AX
	mov	SI,DI
	mov	CX,text_RollHeight
	inc	CX
	mov	DX,CX
	mov	AX,[BP+fillchar]
FILLC:	stosw
	add	DI,158			;(80 - 1) * 2
	loop	FILLC
	mov	CX,DX
	lea	DI,[SI+2000h]
	mov	AX,[BP+fillatr]
FILLA:	stosw
	add	DI,158
	loop	FILLA

	pop	DI
	pop	SI
	pop	BP
	ret	4
endfunc			; }

END
