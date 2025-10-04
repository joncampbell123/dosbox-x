; master library - PC98
;
; Description:
;	SDI�̏풓����������B
;
; Function/Procedures:
;	int sdi_exist( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 = ���݂��Ȃ�
;		1 = ���݂���
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
;	�O���[�o���ϐ� _SdiExists�ɖ߂�l�Ɠ����l���i�[���܂��B
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
	EXTRN SdiExists : WORD

	.CODE

func SDI_EXIST
	mov	AX,37B0h
	int	21h
	sub	AX,4453h	; ����`�`�`
	neg	AX		; �܂�A4453�������� 1,����ȊO�Ȃ� 0��
	sbb	AX,AX		; ���Ă�񂾂��
	inc	AX
	mov	SdiExists,AX
	ret
endfunc

END
