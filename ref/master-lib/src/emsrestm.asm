; master library - EMS - RESTOREPAGEMAP
;
; Description:
;	�y�[�W�}�b�v�̃��X�g�A
;
; Function/Procedures:
;	int ems_restorepagemap( unsigned handle )
;
; Parameters:
;	handle		�I�[�v�����ꂽEMS�n���h��
;
; Returns:
;	0		����
;	80h�`8dh	�G���[(EMS�G���[�R�[�h)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 3.2
;
; Notes:
;	
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
;	93/ 7/19 Initial: emsrestm.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_RESTOREPAGEMAP	; ems_restorepagemap() {
	mov	BX,SP
	; ����
	handle     = (RETSIZE+0)*2

	mov	AH,48h
	mov	DX,SS:[BX+handle]

	int	67h
	mov	AL,AH
	mov	AH,0

	ret	2
endfunc		; }

END
