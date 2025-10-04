; master library - MSDOS
;
; Description:
;	���C���������̏�ʂ���̊m��
;
; Function/Procedures:
;	unsigned mem_allocate( unsigned para ) ;
;
; Parameters:
;	unsigned para	�m�ۂ���p���O���t�T�C�Y
;
; Returns:
;	unsigned �m�ۂ��ꂽDOS�������u���b�N�̃Z�O�����g (cy=0)
;		 0 �Ȃ烁����������Ȃ��̂Ŏ��s�ł�� (cy=1)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 3.0 or later
;
; Notes:
;	AX�ȊO�̃��W�X�^�ی�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/ 2 Initial
;	92/12/11 �Q�ADOS�Ă�łȂ������
;	93/ 1/31 mem_free�� dosfree.asm�Ɉړ�

	.MODEL SMALL
	include func.inc
	.CODE

func MEM_ALLOCATE
	push	BX
	push	CX

	mov	AX,5800h	; �A���P�[�V�����X�g���e�W�𓾂�
	int	21h
	push	AX		; �����X�g���e�W��ۑ�����

	mov	AX,5801h	;
	mov	BX,2		; �ŏ�ʂ���̃A���P�[�V����
	int	21h

	mov	BX,SP
	mov	BX,SS:[BX+(RETSIZE+3)*2]	; para
	mov	AH,48h		; �m�ۂ���
	int	21h
	cmc
	sbb	CX,CX
	and	CX,AX		; CX=seg, ���s�Ȃ� 0

	mov	AX,5801h	; �A���P�[�V�����X�g���e�W�̕��A
	pop	BX		;
	int	21h		;

	mov	AX,CX
	cmp	AX,1		; ���s�� cy=1
	pop	CX
	pop	BX
	ret	2
endfunc

END
