; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMS�������̊J��
;
; Function/Procedures:
;	int ems_free( unsigned handle ) ;
;
; Parameters:
;	unsigned handle ���łɊm�ۂ���Ă���n���h��
;
; Returns:
;	0 ........... success
;	80h�`89h .... failure(EMS �G���[�R�[�h)
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

	.MODEL SMALL
	include func.inc
	.CODE

func	EMS_FREE
	mov	BX,SP
	mov	DX,SS:[BX+RETSIZE*2]
	mov	AH,45h		; function: Deallocate Pages
	int	67h		; call EMM
	mov	AL,AH
	xor	AH,AH
	ret	2
endfunc

END
