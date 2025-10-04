; superimpose & master library module
;
; Description:
;	�p�^�[���̓o�^
;
; Functions/Procedures:
;	int super_entry_pat( int patsize, void far *image_addr, int clear_color ) ;
;
; Parameters:
;	patsize
;	image_addr	�p�^�[���̐擪�A�h���X
;	clear_color	�����F
;
; Returns:
;	InsufficientMemory	(cy=1)	������������Ȃ�
;	GeneralFailure		(cy=1)	�o�^������������
;	0�`			(cy=0)	�����B�o�^�����p�^�[���ԍ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	Heap���烁�������p�^�[����ێ����邽�߂Ɏ擾���܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: superpat.asm 0.09 93/02/19 20:11:35 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/12/ 6 [M0.22] BUGFIX �p�^�[����S���폜�������Ƃ���
;			super_buffer���Ċm�ۂ��Ă���
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/14 [M0.22k] BUGFIX ���s���AGeneralFailure�����Ԃ��Ă��Ȃ�����
;	95/ 3/16 [M0.22k] BUGFIX ���s���A���������J�����Ă��Ȃ�����

	.186
	.MODEL SMALL

	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patnum:WORD		; superpa.asm
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE

	EXTRN	SUPER_ENTRY_AT:CALLMODEL	; superat.asm
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL	; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL		; memheap.asm

MRETURN	macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

retfunc ENTRY_AT_ERROR	; �G���[	in: ES = pattern segment
	push	AX
	push	ES
	call	HMEM_FREE
	pop	AX
	jmp	short ERROR_EXIT
endfunc

retfunc NO_MEMORY	; �������s��
	mov	AX,InsufficientMemory

ERROR_EXIT label near	; �G���[�I��	in: AX = error code
	stc
	MRETURN
endfunc

func SUPER_ENTRY_PAT	; super_entry_pat() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	patsize		= (RETSIZE+4)*2
	image_addr	= (RETSIZE+2)*2
	clear_color	= (RETSIZE+1)*2

	mov	DI,super_patnum
	shl	DI,1			;integer size
	mov	AX,[BP+patsize]
	mov	DX,AX
	mul	AH
	mov	BX,AX	; BX = plane size
	shl	AX,2
	add	AX,BX
	mov	mem_AllocID,MEMID_super
	push	AX
	_call	HMEM_ALLOCBYTE		; allocate (plane size * 5) bytes
	jc	short NO_MEMORY

	mov	ES,AX			; ES = �p�^�[���̈�

	push	super_patnum	; �o�^�ԍ�
	push	DX		; patsize
	push	AX		; �p�^�[���̈�
	_call	SUPER_ENTRY_AT		; BX,ES�͔j�󂳂�Ȃ�����
	jc	short ENTRY_AT_ERROR

	push	DS

	; �p�^�[���f�[�^���m�ۃp�^�[���̈�ɓ]������

	lds	SI,[BP+image_addr]
	mov	DI,BX
	mov	CX,BX
	shl	CX,1			;4plane / word
	rep	movsw
	push	ES
	pop	DS			; DS ���p�^�[���̈��

	; �}�X�N�v���[���𐶐�����

	mov	SI,BX			; plane size (�p�^�[���擪)
	mov	DX,[BP+clear_color]
	mov	DH,DL

	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	jc	short BLUE_REV
	rep	movsb
	jmp	short RED
	EVEN
BLUE_REV:
	lodsb
	not	AL
	stosb
	loop	short BLUE_REV
	EVEN
RED:
	; red
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_RED:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_RED

	; green
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_GREEN:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_GREEN

	; inten
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_INTEN:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_INTEN

	; �p�^�[�����̓����������}�X�N�ŌJ�蔲��

	test	DL,DL			; �����F�� 0 �Ȃ�ȗ�
	jz	short return

	mov	AH,4
	EVEN
CLEAR:
	xor	SI,SI
	mov	CX,BX
	EVEN
CLEAR_PLANE:
	lodsb
	and	[DI],AL
	inc	DI
	loop	short CLEAR_PLANE
	dec	AH
	jnz	short CLEAR
	EVEN

	; �I���
return:
	pop	DS
	mov	AX,super_patnum
	dec	AX
	MRETURN
endfunc			; }

END
