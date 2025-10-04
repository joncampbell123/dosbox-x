; master library - font
;
; Description:
;	2�o�C�g���p�t�H���g�� ANK�t�H���g�Ƃ��ēo�^����
;
; Function/Procedures:
;	void font_entry_kcg(void) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal Mode
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�o�^����͈͂́A20h�`7eh, a1h�`dfh�ł��B����ȊO�̒l�͐ݒ�O
;	(�m�ے���Ȃ烉���_���p�^�[��)�̂܂܂ł��B
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/11/21 Initial: fontentk.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc
	EXTRN FONT_WRITE:CALLMODEL
	EXTRN FONT_READ:CALLMODEL

	.CODE

func FONT_ENTRY_KCG	; font_entry_kcg() {
	push	BP
	mov	BP,SP
	sub	SP,32
	push	SI
	push	DI

	temp = -32

	; space
	push	SS
	pop	ES
	lea	DI,[BP+temp]
	mov	CX,16/2
	xor	AX,AX
	rep	stosw

	mov	AX,32	; space
	push	AX
	_push	SS
	lea	AX,[BP+temp]
	push	AX
	call	FONT_WRITE

	; !����̃��[�v
	mov	SI,33
	mov	DI,2921h
	jmp	short GO
KLOOP:
	mov	SI,0a1h
GO:
	push	DI
	_push	SS
	lea	AX,[BP+temp]
	push	AX
	call	FONT_READ

	push	SI
	_push	SS
	lea	AX,[BP+temp]
	push	AX
	call	FONT_WRITE

	inc	DI
	cmp	DI,297fh
	jne	short SKIP
	mov	DI,2a21h
SKIP:
	inc	SI
	cmp	SI,7fh
	je	short KLOOP
	cmp	SI,0e0h
	jl	short GO

	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret
endfunc		; }

END
