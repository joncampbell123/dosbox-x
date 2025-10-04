; Master Library - XMS
;
; Description:
;	XMS���������Ċm�ۂ���
;
; Functions:
;	unsigned xms_reallocate( unsigned handle, long memsize )
;
; Parameters:
;	handle   ���łɊm�ۂ��ꂽ XMS�n���h��
;	memsize  �V�����o�C�g��
;
; Returns:
;	0 = ���s
;	1 = ����
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
;	93/ 7/16 Initial: xmsrealc.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_REALLOCATE	; xms_reallocate() {
	mov	BX,SP
	; ����
	handle  = (RETSIZE+2)*2
	memsize = (RETSIZE+0)*2

	mov	AX,SS:[BX+memsize]
	mov	DX,SS:[BX+memsize+2]
	mov	CX,1024
	div	CX
	add	DX,-1
	adc	AX,0
	mov	DX,SS:[BX+handle]
	mov	BX,AX

	mov	AH,0fh
	call	[xms_ControlFunc]
	ret 6
endfunc			; }

END
