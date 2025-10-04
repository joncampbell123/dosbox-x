; grcg - PC98V
;
; Function:
;	void far _pascal grcg_and( int mode, int color ) ;
;	void far _pascal grcg_or( int mode, int color ) ;
;
; Description:
;	�E�O���t�B�b�N�`���[�W���[�̃��[�h�� "�w��F�ł�AND"�ɂɐݒ肷��B
;	�E�V�A"�w��F�ł�OR"�ɐݒ肷��B
;
; Parameters:
;	int mode	���[�h�B
;	int color	�F�B0..15
;
; Binding Target:
;	Microsoft-C / Turbo-C / etc...
;
; Running Target:
;	PC-9801V Normal Mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRAPHIC CHAGER
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0�݊��Ȃ�� OK )
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/ 7/ 1 Initial
;	95/ 3/25 [M0.22k] grcg_and �����������삵�Ă��Ȃ�����

	.MODEL SMALL
	.CODE
	include func.inc

	extrn GRCG_SETCOLOR : callmodel

; void _pascal grcg_and( int mode, int color ) ;
func GRCG_AND
	push	BP
	mov	BP,SP

	; parameters
	mode  = (RETSIZE+2)*2
	color = (RETSIZE+1)*2

	mov	AX,[BP+mode]
	mov	BX,[BP+color]

	and	BX,0Fh
	or	AX,BX
	;xor	BX,0Fh
	push	AX
	push	BX
	call	GRCG_SETCOLOR

	pop	BP
	ret	4
endfunc

; void _pascal grcg_or( int mode, int color ) ;
func GRCG_OR
	push	BP
	mov	BP,SP

	; parameters
	mode  = (RETSIZE+2)*2
	color = (RETSIZE+1)*2

	mov	AX,[BP+mode]
	mov	BX,[BP+color]

	and	BX,0Fh
	mov	CX,BX
	xor	CX,0Fh
	or	AX,CX
	push	AX
	push	BX
	call	GRCG_SETCOLOR

	pop	BP
	ret	4
endfunc

END
