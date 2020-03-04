; master library - text - scroll - left - character
;
; Description:
;	テキスト画面の左スクロール
;	属性なし
;
; Function/Procedures:
;	void text_roll_left_c(unsigned fillchar );
;
; Parameters:
;	fillchar	新しく現れた部分を埋める文字
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
;	94/ 3/30 Initial: txrllec.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD

	.CODE

func TEXT_ROLL_LEFT_C	; text_roll_left_c() {
	push	BP
	mov	BP,SP
	; 引数
	fillchar = (RETSIZE+1)*2
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
	add	SI,BX
	add	DI,BX
	dec	DX
	jg	short LOOPY

	shl	AX,1
	mov	DI,text_RollTopOff
	add	DI,AX
	mov	CX,text_RollHeight
	inc	CX
	mov	AX,[BP+fillchar]
FILLC:	stosw
	add	DI,158			;(80 - 1) * 2
	loop	FILLC

	pop	DI
	pop	SI
	pop	BP
	ret	2
endfunc			; }

END
