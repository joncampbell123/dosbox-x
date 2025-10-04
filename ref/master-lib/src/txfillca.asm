; master library - PC98
;
; Description:
;	�e�L�X�g��ʑS�̂��w�蕶���E�w�葮���Ŗ��߂�
;
; Function/Procedures:
;	void text_fillca( unsigned chr, unsigned atrb ) ;
;
; Parameters:
;	unsigned chr	����(���݂� ANK�̂�)
;	unsigned atrb	����
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
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
;	92/11/15 Initial
;	92/12/22 ���[�Ashl xx,1��4�s�����Ă��̂ŁACL=4,shl xx,CL�ɂ����̂�

	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN TextVramSeg : WORD
	.CODE

func TEXT_FILLCA
	mov	BX,BP
	mov	BP,SP
	push	DI

	; ����
	chr	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:0712h	; �s��
	inc	AX
	mov	DX,AX
	shl	DX,1
	shl	DX,1
	add	DX,AX
	mov	CL,4
	shl	DX,CL		; DX = �s�� * 80
	mov	CX,DX

	mov	ES,TextVramSeg
	xor	DI,DI
	mov	AX,[BP + chr]
	rep	stosw

	mov	CX,DX
	mov	DI,2000h
	mov	AX,[BP + atrb]
	rep	stosw

	pop	DI
	mov	BP,BX
	ret	4
endfunc

END
