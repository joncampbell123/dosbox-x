; master library - PC/AT
;
; Description:
;	�J�[�\���`��𓾂�B
;
; Procedures/Functions:
;	unsigned vtext_getcursor( void );
;
; Parameters:
;	none
;
; Returns:
;	�J�[�\���`��
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
;		 get_cursor -> vtext_getcursor
;	94/ 4/10 Initial: vtsetcur.asm/master.lib 0.23

	.MODEL SMALL
	.DATA

	include func.inc

	.CODE

func VTEXT_GETCURSOR	; vtext_getcursor() {
	mov	AH,03h
	xor	BH,BH
	int	10h	; get cursor type & cursor pos.
	mov	AX,CX
	ret
endfunc			; }

END
