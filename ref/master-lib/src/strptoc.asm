; master library
;
; Description:
;	�p�X�J����������b������ɕϊ�����
;
; Function/Procedures:
;	char * str_pastoc( char * CString, const char * PascalString ) ;
;
; Parameters:
;	char * CString		�ϊ�����(�b������)�̊i�[��
;	char * PascalString	Pascal������ւ̃A�h���X
;
; Returns:
;	char *			�ϊ����ꂽ������ւ̃A�h���X
;				(CString�����̂܂ܕԂ����)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PascalString = CString�ł����삵�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/22 Initial
;	93/ 7/16 [M0.20] BUGFIX ��! ret �ŊJ�����Ă�ʂ�����Ȃ�����(^^;;

	.MODEL SMALL
	include func.inc

	.CODE

func STR_PASTOC
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ����
	CString      = (RETSIZE + 1 + DATASIZE)*2
	PascalString = (RETSIZE + 1)*2

	_lds	SI,[BP+PascalString]
	_les	DI,[BP+CString]
	s_mov	DX,DS
	s_mov	ES,DX
	_mov	DX,ES
	mov	BX,DI

	CLD
	xor	AX,AX	; �����𓾂�
	lodsb
	mov	CX,AX

	rep	movsb
	mov	AL,0
	stosb

	mov	AX,BX	; DXAX = PascalString

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret DATASIZE*4
endfunc

END
