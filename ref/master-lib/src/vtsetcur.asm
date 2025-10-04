; master library - PC/AT
;
; Description:
;	�J�[�\���̌`���ύX����B
;
; Procedures/Functions:
;	void vtext_setcursor( unsigned shape )
;
; Parameters:
;	shape
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
;	OPTASM 1.6
;
; Author:
;	�̂�/V(H.Maeda)
;
; Revision History:
;	93/10/10 Initial
;	94/01/16 �֐����̕ύX
;		 set_cursor -> vtext_setcursor
;	94/ 4/10 Initial: vtsetcur.asm/master.lib 0.23
;	94/ 5/20 �`�󂪕ω����Ȃ��ꍇ�͖�������悤�ɂ���

	.MODEL SMALL
	include func.inc

	.DATA

	.CODE

func VTEXT_SETCURSOR	; vtext_setcursor() {
	cursor	= (RETSIZE+0)*2

	mov	AH,03h
	xor	BH,BH
	int	10h	; get cursor type & cursor pos.
	mov	AX,CX

	mov	BX,SP
	mov	CX,SS:[BX+cursor]	; cursor
	cmp	AX,CX
	je	short IGNORE

	mov	AH,01h		; set cursor type
	int	10h

IGNORE:
	ret	2
endfunc			; }

END

