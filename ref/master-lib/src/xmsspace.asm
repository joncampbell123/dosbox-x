; master library - MS-DOS - XMS
;
; Description:
;	XMS�̎c�胁�����̑��o�C�g���𓾂�
;
; Function/Procedures:
;	unsigned long xms_space( void )
;
; Parameters:
;	none
;
; Returns:
;	0    �G���[(�n���h���������A�h���C�o�����̃t�@���N�V������m��Ȃ�)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 80286
;
; Notes:
;	xms_exist�ő��݂ƕԂ���Ă��Ȃ���ԂŎ��s����Ɩ\�����܂��B
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
;	93/ 7/16 Initial: xmsspace.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_SPACE	; xms_space() {
	mov	AH,08h
	call	[xms_ControlFunc]
	mov	AX,1024
	mul	DX
	ret
endfunc		; }

END
