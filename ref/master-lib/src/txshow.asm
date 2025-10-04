; master library - PC98
;
; Description:
;	�e�L�X�g��ʂ�\������
;
; Function/Procedures:
;	void text_show( void ) ;
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
;	93/ 1/16 Initial
;	94/ 1/ 8 [M0.22] TextShown�ɐݒ�

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextShown:WORD

	.CODE

func TEXT_SHOW
	mov	AH,0ch
	int	18h
	mov	TextShown,1
	ret
endfunc

END
