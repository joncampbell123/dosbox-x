; master library
;
; Description:
;	テキスト画面の上スクロール
;	属性あり
;
; Function/Procedures:
;	void text_roll_up_c( int fillchar, int fillatr ) ;
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/20 Initial


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD

	.CODE

func TEXT_ROLL_UP_CA	; text_roll_up_ca() {
	push	BP
	mov	BP,SP
	; 引数
	fillchar = (RETSIZE+2)*2
	fillatr = (RETSIZE+1)*2
	push	SI
	push	DI

	mov	DI,text_RollTopOff
	mov	DX,text_RollHeight
	lea	SI,[DI+160]
	mov	AX,text_RollWidth
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

	mov	CX,AX
	mov	DX,AX
	mov	AX,[BP+fillchar]
	rep	stosw
	mov	CX,DX
	lea	DI,[DI+BX+2000h-160]
	mov	AX,[BP+fillatr]
	rep	stosw

	pop	DI
	pop	SI
	pop	BP
	ret	4
endfunc			; }

END
