; master library - vtext
;
; Description:
;	�w��͈͂̕��������ύX
;
; Function/Procedures:
;	void vtext_boxfilla( int x1, int y1, int x2, int y2 , unsigned attr );
;
; Parameters:
;	x1,y1	����̍��W( (0,0)=����� )
;	x2,y2	�E���̍��W( (?,?)=�E���� )
;	attr	��������(���8bit=and mask, ����8bit=xor mask)
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/10/10 Initial
;	93/10/25 vtext_attribute -> vtext_boxfilla
;	93/11/ 7 VTextState ����
;	94/ 4/ 9 Initial: vtboxfla.asm/master.lib 0.23
;	94/ 5/20 �������̑����̎d�l�ύX

ATTR_NO_CNG	EQU	01H
ATTR_ADD_HL	EQU	02H
ATTR_DEL_HL	EQU	04H

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramWidth : WORD
	EXTRN TextVramSize : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_BOXFILLA	; vtext_boxfilla() {
	push	BP
	mov	BP,SP
	; ����
	x1	= (RETSIZE+5)*2
	y1	= (RETSIZE+4)*2
	x2	= (RETSIZE+3)*2
	y2	= (RETSIZE+2)*2
	attr	= (RETSIZE+1)*2

	CLD
	push	SI
	push	DI

	mov	CX,[BP+y2]
	mov	AX,[BP+y1]	; ax = y1
	sub	CX,AX		; cx = y2 - y1
	mul	TextVramWidth	; DX:AX= y1 * Width

	mov	DX,CX	; dx = y2 - y2

	mov	BX,[BP+x1]
	add	AX,BX
	shl	AX,1
	les	DI,TextVramAdr
	add	DI,AX	; es:di = (y1 * 80 + x1) * 2

	mov	AX,[BP+x2]
	sub	AX,BX
	inc	AX	; ax = x2 - x1 + 1
	mov	SI,AX

	mov	BX,TextVramWidth
	sub	BX,AX	; bx = Width - (x2 - x1 + 1)
	shl	BX,1	; bx =(Width - (x2 - x1 + 1) )*2

	mov	BP,[BP+attr]
LOOPY:
	mov	CX,SI
LOOPX:
	inc	DI		; skip character code area
	mov	AX,BP
	and	AH,ES:[DI]
	xor	AL,AH
	stosb		; move es:[di],al
	loop	short LOOPX

	test	byte ptr VTextState,1
	jne	short NO_REF
	mov	CX,SI
	sub	DI,SI
	sub	DI,SI
	mov	AH,0ffh		; Refresh Screen
	int	10h
	add	DI,SI
	add	DI,SI
NO_REF:

	lea	DI,[DI+BX]
	dec	DX
	jg	short LOOPY

	pop	DI
	pop	SI
	pop	BP
	ret	10
endfunc

END
