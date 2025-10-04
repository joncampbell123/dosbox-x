; master library - EMS - MAPHANDLEPAGE
;
; Description:
;	�n���h���y�[�W�̃}�b�v
;
; Function/Procedures:
;	int ems_maphandlepage( int phys_page, unsigned handle, unsigned log_page )
;
; Parameters:
;	phys_page	�����y�[�W�t���[���ԍ�(0�`)
;	handle		�I�[�v�����ꂽEMS�n���h��
;	log_page	EMS�n���h�����̑��΃y�[�W�ԍ�(0�`)
;
; Returns:
;	0		����
;	80h�`8bh	�G���[(EMS�G���[�R�[�h)
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
;	93/ 7/19 Initial: emsmaphp.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_MAPHANDLEPAGE	; ems_maphandlepage() {
	push	BP
	mov	BP,SP
	; ����
	phys_page  = (RETSIZE+3)*2
	handle     = (RETSIZE+2)*2
	log_page   = (RETSIZE+1)*2

	mov	AH,44h
	mov	AL,[BP+phys_page]
	mov	BX,[BP+log_page]
	mov	DX,[BP+handle]

	int	67h
	mov	AL,AH
	mov	AH,0

	pop	BP
	ret	6
endfunc		; }

END
