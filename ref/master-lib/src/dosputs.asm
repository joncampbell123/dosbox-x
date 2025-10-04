; master library - MS-DOS
;
; Description:
;	�W���o�͂ɕ�������o�͂���
;
; Function/Procedures:
;	void dos_puts( const char * strp ) ;
;
; Parameters:
;	char * strp	������
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
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
;	92/11/17 Initial
;	93/ 1/16 SS!=DS�Ή�
;	95/ 2/15 [M0.22k] CLD�ǉ�

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTS	; dos_puts() {
	mov	BX,SP
	push	DI
	_push	DS

	; ����
	strp	= RETSIZE * 2

	_lds	DX,SS:[BX+strp]
	mov	AX,DS
	mov	ES,AX

	CLD
	mov	DI,DX
	mov	AX,4000h	; 40h: DOS WRITE
	mov	CX,-1
	repne	scasb
	not	CX
	dec	CX		; CX = strlen(strp) ;
	mov	BX,1		; BX = 1(STDOUT)
	int	21h

	_pop	DS
	pop	DI
	ret	DATASIZE*2
endfunc		; }

END
