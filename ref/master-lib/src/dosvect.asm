; master library - MS-DOS
;
; Description:
;	���荞�݃x�N�^�̓ǂݎ��ƃt�b�N
;
; Function/Procedures:
;	void far * dos_setvect( int vect, void far * address ) ;
;
; Parameters:
;	int vect	Interrupt Number 0�`255
;	void far * address �ݒ肷�銄�荞�݃��[�`���̃A�h���X
;
; Returns:
;	void far *	INT vect�ɐݒ肳��Ă������荞�݃��[�`��
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
; Assembly Language Note:
;	�A�Z���u������g���ꍇ�A���̂悤�Ɏg���Ă��������B
;	push	VECTOR		; �Ăяo��
;	push	SEGMENT
;	push	OFFSET
;	call	DOS_SETVECTOR
;	mov	SAVE_SEG,DX	; ���x�N�^�̑ޔ�
;	mov	SAVE_OFF,AX
;	���W�X�^�́AAX,DX�ȊO�͕ۑ�����܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/18 Initial
;	92/11/21 bugfix(for asm)

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_SETVECT
	push	BP
	mov	BP,SP
	push	DS

	push	BX		; �A�Z���u�����[�`���̂��߂�
	push	ES		; ���W�X�^��ۑ�����

	; ����
	vect	= (RETSIZE+3)*2
	address = (RETSIZE+1)*2

	mov	AL,[BP+vect]
	lds	DX,[BP+address]

	mov	AH,35h
	int	21h		; read vector -> ES:BX
	mov	AH,25h		; set vector <- DS:DX
	int	21h

	mov	AX,BX
	mov	DX,ES

	pop	ES		; �A�Z���u���p�ɕۑ����ꂽ���W�X�^�̕���
	pop	BX		;

	pop	DS
	pop	BP
	ret	6
endfunc

END
