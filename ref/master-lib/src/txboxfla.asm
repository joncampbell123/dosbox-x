; master library - PC98
;
; Description:
;	�e�L�X�g��ʂ̒����`�͈͂��w�葮���Ŗ��߂�
;
; Function/Procedures:
;	void text_boxfilla( unsigned x1, unsigned y1, unsigned x2, unsigned y2,
;				unsigned atrb ) ;
;
; Parameters:
;	unsigned x1	���[�̉����W( 0 �` 79 )
;	unsigned y1	��[�̏c���W( 0 �` 24 )
;	unsigned x2	�E�[�̉����W( 0 �` 79 )
;	unsigned y2	���[�̏c���W( 0 �` 24 )
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
;	x1 <= x2 ���� y1 <= y2�łȂ���΂Ȃ�Ȃ��B
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


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_BOXFILLA
	push	DI
	pushf
	CLI
	add	SP,(2+retsize)*2
			; �p�����[�^�̈�̐擪( flags,DI,retadr )

	pop	AX	; color
	pop	DX	; y2
	pop	BX	; x2
	pop	CX	; Y1

	mov	DI,CX
	shl	DI,1
	shl	DI,1
	add	DI,CX
	shl	DI,1
	add	DI,TextVramSeg
	add	DI,200h
	mov	ES,DI	; ����� ES�� (0,y1)�̃e�L�X�g�����Z�O�����g��������

	pop	DI	; X1

	sub	SP,(5+2+retsize)*2
			; (5(���Ұ��̐�)+1(���ݱ��ڽ)+2(DI,flags))*2
	popf

	sub	BX,DI	; BX = X2 - X1 + 1
	inc	BX	;

	sub	DX,CX
	mov	CX,DX
	shl	DX,1
	shl	DX,1
	add	DX,CX
	shl	DX,1
	shl	DX,1
	shl	DX,1
	shl	DX,1
	add	DI,DX
	shl	DI,1	; ����� DI �ɂ�(x1,y2)��offset���������B

	lea	DX,[BX+80]
	shl	DX,1	; ����̌�����

	EVEN
SLOOP:		mov     CX,BX
		rep stosw	; ��s���̏�������
		sub     DI,DX	; ���̍s�Ɉړ�
	jnb	short SLOOP

	pop	DI
	ret	10
endfunc

END
