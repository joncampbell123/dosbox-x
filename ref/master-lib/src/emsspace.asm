; master library - MSDOS - EMS
;
; Description:
;	EMS�̎c�胁�����̑��o�C�g���𓾂�
;
; Function/Procedures:
;	unsigned long ems_space( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	0     �G���[
;	�ȊO  �m�ۂ���Ă���o�C�g��
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
;	93/ 7/16 Initial: emssize.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_SPACE ; ems_space() {
	mov	AH,42h		; function: Get Unallocated Pages
	int	67h		; call EMM
	xor	DX,DX
	cmp	AH,0
	mov	AX,DX
	jne	short DAME
	mov	DX,BX
	shr	DX,1
	rcr	AX,1
	shr	DX,1
	rcr	AX,1
DAME:
	ret
endfunc		; }

END
