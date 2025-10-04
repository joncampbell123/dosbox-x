; master library - MSDOS - EMS
;
; Description:
;	EMS�������̊m��
;
; Function/Procedures:
;	int ems_reallocate( unsigned handle, unsigned long size ) ;
;
; Parameters:
;	handle  ���Ɋm�ۂ���Ă��� EMS �n���h��
;	size	�V�����o�C�g��
;
; Returns:
;	int	0     ����
;		else  EMS�G���[�R�[�h
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
;	93/ 7/17 Initial: emsrealc.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_REALLOCATE	; ems_reallocate() {
	mov	BX,SP
	; ����
	handle	= (RETSIZE+2)*2
	size_hi	= (RETSIZE+1)*2
	size_lo	= (RETSIZE+0)*2

	mov	DX,SS:[BX+handle]
	mov	CX,SS:[BX+size_lo]
	mov	BX,SS:[BX+size_hi]
	shl	CX,1
	rcl	BX,1
	shl	CX,1
	rcl	BX,1
	cmp	CX,1
	sbb	BX,-1
	mov	AH,51h		; function: Reallocate Pages
	int	67h		; call EMM
	mov	AL,AH
	xor	AH,AH
	ret	6
endfunc		; }

END
