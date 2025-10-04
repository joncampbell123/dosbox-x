; master library - PC98 - text - boxfill - character - attribute
;
; Description:
;	�e�L�X�g��ʂ̒����`�͈͂��w�蕶���E�w�葮���Ŗ��߂�
;
; Functions/Procedures:
;	void text_boxfillca(unsigned x1, unsigned y1, unsigned x2, unsigned y2,
;				unsigned fillchar, unsigned atrb ) ;
;
; Parameters:
;	unsigned x1	���[�̉����W( 0 �` 79 )
;	unsigned y1	��[�̏c���W( 0 �` )
;	unsigned x2	�E�[�̉����W( 0 �` 79 )
;	unsigned y2	���[�̏c���W( 0 �` )
;	unsigned fillchar ����
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
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;	Kazumi(���c  �m)
;
; Revision History:
;	92/11/15 Initial: txboxfla.asm
;	94/ 4/ 6 Initial: txboxfca.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_BOXFILLCA	; text_boxfillca() {
	push	SI
	push	DI
	pushf
	CLI
	add	SP,(3+retsize)*2
			; �p�����[�^�̈�̐擪( flags,DI,retadr )

	pop	AX	; color
	pop	SI	; char
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

	sub	SP,(6+3+retsize)*2
			; (6(���Ұ��̐�)+1(���ݱ��ڽ)+3(SI,DI,flags))*2
	popf

	push	SI

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
	mov	SI,DI

	lea	DX,[BX+80]
	shl	DX,1	; ����̌�����

	EVEN
ALOOP:		mov     CX,BX
		rep stosw	; ��s���̏�������
		sub     DI,DX	; ���̍s�Ɉړ�
	jnb	short ALOOP

	mov	AX,ES
	sub	AX,200h
	mov	ES,AX		; �e�L�X�g�Z�O�����g
	mov	DI,SI
	pop	AX
	EVEN
CLOOP:		mov     CX,BX
		rep stosw	; ��s���̏�������
		sub     DI,DX	; ���̍s�Ɉړ�
	jnb	short CLOOP

	pop	DI
	pop	SI
	ret	12
endfunc			; }

END
