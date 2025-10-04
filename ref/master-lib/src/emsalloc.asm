; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMS�������̊m��
;
; Function/Procedures:
;	unsigned ems_allocate( unsigned long size ) ;
;
;
; Parameters:
;	unsigned long size	�m�ۂ���o�C�g���B
;
; Returns:
;	int	0 ... ���s
;		else  EMS�n���h��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 3.2
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
;	92/11/16 Initial
;	92/12/15 �v�Z�����傢�Ɖ���

	.MODEL SMALL
	include func.inc
	.CODE

func EMS_ALLOCATE
	mov	BX,SP
	mov	CX,SS:[BX+(RETSIZE)*2]		; lo
	mov	BX,SS:[BX+(RETSIZE)*2+2]	; hi
	shl	CX,1
	rcl	BX,1
	shl	CX,1
	rcl	BX,1
	cmp	CX,1
	sbb	BX,-1
	mov	AH,43h		; function: Allocate Pages
	int	67h		; call EMM
	sub	AH,1
	sbb	AX,AX
	and	AX,DX
	ret	4
endfunc

END
