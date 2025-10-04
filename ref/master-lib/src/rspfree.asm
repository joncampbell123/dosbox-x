; master library - PC98
;
; Description:
;	�풓�p���b�g�̊J��
;
; Function/Procedures:
;	void respal_free( void ) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�풓�p���b�g���쐬����Ă��Ȃ���΁A�������܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN ResPalSeg:WORD

	.CODE
	EXTRN RESPAL_EXIST:CALLMODEL

func RESPAL_FREE
	mov	AX,ResPalSeg
	or	AX,AX
	jnz	short FREE
	call	RESPAL_EXIST
	or	AX,AX
	jnz	short IGNORE
FREE:
	mov	ES,AX
	mov	AH,49h	; �������u���b�N�̊J��
	int	21h

	mov	ResPalSeg,0
IGNORE:
	ret
endfunc

END
