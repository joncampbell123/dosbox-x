; Master Library - XMS
;
; Description:
;	XMS���������J������
;
; Functions:
;	void xms_free( unsigned handle )
;
; Parameters:
;	unsigned handle	XMS�������n���h��
;
; Returns:
;	none
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/23 Initial
;	93/ 7/16 [M0.20] _xms_ControlFunc -> xms_ControlFunc�ɉ���

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_FREE
	mov	BX,SP
	; ����
	handle = (RETSIZE)*2

	mov	DX,SS:[BX+handle]
	mov	AH,0ah
	call	[xms_ControlFunc]
	ret 2
endfunc

END
