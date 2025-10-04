; master library - MSDOS - EMS
;
; Description:
;	�w��̖��O������EMS�������n���h����T��
;
; Function/Procedures:
;	unsigned ems_findname( const char * hname );
;
; Parameters:
;	char * hname	8bytes,�]���Ă���ꍇ��'\0'�Ŗ��߂邱��
;
; Returns:
;	0	������Ȃ�
;	1�`	�������� EMS�n���h��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 4.0
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
;	93/ 7/19 Initial: emsfindn.asm/master.lib 0.20
;	94/ 3/12 [M0.23] ������Ȃ������Ƃ���0��Ԃ��ĂȂ�����

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_FINDNAME	; ems_findname() {
	push	SI
	mov	SI,SP
	_push	DS

	; ����
	hname	= (RETSIZE+1)*2

	_lds	SI,SS:[SI+hname]

	mov	AX,5401h	; function: Set Handle Name
	int	67h		; call EMM
	test	AH,AH
	mov	AX,0
	jnz	short	NOTFOUND
	mov	AX,DX
NOTFOUND:
	_pop	DS
	pop	SI
	ret	DATASIZE*2
endfunc			; }

END
