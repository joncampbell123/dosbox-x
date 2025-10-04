; master library - superimpose
;
; Description:
;	�p�^�[���̓o�^(�ŉ���)
;
; Function/Procedures:
;	int super_entry_at( int num, unsigned patsize, unsigned patseg ) ;
;
; Parameters:
;	num	�o�^��̃p�^�[���ԍ�
;	patsize	�p�^�[���̑傫��
;	seg	�p�^�[���f�[�^�̊i�[���ꂽ, hmem�u���b�N�Z�O�����g�A�h���X
;		(���̃A�h���X�����̂܂ܓo�^�����B���g�̓p�^�[���T�C�Y��
;		 �]���� mask->B->R->G->I�̏��ɓ����Ă��邱��, �܂���
;		 TINY�`������)
;
; Returns:
;	InsufficientMemory	(cy=1) super_buffer���m�ۂł��Ȃ�(9216bytes)
;	GeneralFailure		(cy=1) num�� 512�ȏ�
;	NoError			(cy=0) ����
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	���łɓo�^����Ă���ꏊ�ɓo�^����ƁA�ȑO�̃f�[�^�͊J������܂��B
;
; Assembly Language Note:
;	AX,flag���W�X�^��j�󂵂܂��B
;	d flag�͕K�� 0 �ɂȂ�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/12/ 7 Initial: superat.asm/master.lib 0.22
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patdata:WORD		; superpa.asm
	EXTRN	super_patsize:WORD
	EXTRN	super_buffer:WORD
	EXTRN	super_patnum:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	HMEM_ALLOC:CALLMODEL		; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL


BUFFER_SIZE equ (16+2)*128*4

func SUPER_ENTRY_AT	; super_entry_at() {
	push	BP
	mov	BP,SP
	push	BX

	CLD

	num	= (RETSIZE+3)*2
	patsize	= (RETSIZE+2)*2
	patseg	= (RETSIZE+1)*2

	mov	BX,[BP+num]
	cmp	BX,MAXPAT
	cmc
	mov	AX,GeneralFailure
	jc	short EXIT

	; super_buffer���`�F�b�N
	cmp	super_buffer,0
	jne	short DO_ENTRY
	; ���S�ɊJ�����ꂽ���(�����l)�������ꍇ

	mov	mem_AllocID,MEMID_super
	push	BUFFER_SIZE/16
	_call	HMEM_ALLOC	; �m�ۂ���
	mov	super_buffer,AX
	mov	AX,InsufficientMemory
	jc	short EXIT

	push	ES
	push	CX
	push	DI

	push	DS
	pop	ES
	xor	AX,AX
	mov	DI,offset super_patsize	; �p�^�[���T�C�Y�z���������
	mov	CX,MAXPAT
	rep	stosw

	pop	DI
	pop	CX
	pop	ES

DO_ENTRY:
	mov	AX,BX
	shl	BX,1

	cmp	AX,super_patnum
	jae	short ADDING		; �Ō���ȍ~�Ȃ�c

OVERWRITING:				; �o�^�Ō�����O�Ȃ�㏑��
	cmp	super_patsize[BX],0
	je	short GO

	push	super_patdata[BX]	; ���łɉ�������Ȃ�J��
	_call	HMEM_FREE
	jmp	short GO

ADDING:
	inc	AX
	mov	super_patnum,AX

GO:
	mov	AX,[BP+patsize]
	mov	super_patsize[BX],AX
	mov	AX,[BP+patseg]
	mov	super_patdata[BX],AX
	xor	AX,AX			; clc, NoError

EXIT:
	pop	BX
	pop	BP
	ret	6
	EVEN
endfunc		; }

END
