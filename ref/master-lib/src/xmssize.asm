; master library - MS-DOS - XMS
;
; Description:
;	XMS�n���h���̊m�ۂ��ꂽ�o�C�g���𓾂�
;
; Function/Procedures:
;	unsigned long xms_size( unsigned handle )
;
; Parameters:
;	handle   ���łɊm�ۂ���Ă��� XMS �n���h��
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
;	93/ 7/16 Initial: xmssize.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN xms_ControlFunc:DWORD

	.CODE

func XMS_SIZE	; xms_size() {
	mov	BX,SP
	; ����
	handle = (RETSIZE)*2

	mov	DX,SS:[BX+handle]
	mov	AH,0eh
	call	[xms_ControlFunc]
	neg	AX
	and	AX,1024
	mul	DX
	ret 2
endfunc		; }

END
