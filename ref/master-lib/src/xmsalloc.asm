; Master Library - XMS
;
; Description:
;	XMS���������m�ۂ���
;
; Functions:
;	unsigned xms_allocate( long memsize )
;
; Parameters:
;	memsize  �m�ۂ������o�C�g��
;
; Returns:
;	0 = ���s
;	else XMS�������n���h��
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
;	93/ 1/22 bugfix (thanks,Mikio!)
;	93/ 7/16 [M0.20] _xms_ControlFunc -> xms_ControlFunc�ɉ���

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_ALLOCATE
	mov	BX,SP
	; ����
	memsize = (RETSIZE)*2

	mov	AX,SS:[BX+memsize]
	mov	DX,SS:[BX+memsize+2]
	mov	CX,1024
	div	CX
	add	DX,-1
	adc	AX,0
	mov	DX,AX

	mov	AH,09h
	call	[xms_ControlFunc]
	shr	AX,1
	sbb	AX,AX
	and	AX,DX
	ret 4
endfunc

END
