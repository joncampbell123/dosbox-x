; master library - 
;
; Description:
;	�p�^�[���̃R�s�[�̍쐬
;
; Functions/Procedures:
;	int super_duplicate( int tonum, int fromnum ) ;
;
; Parameters:
;	tonum			�o�^��p�^�[���ԍ�
;	fromnum			���ʌ��p�^�[��
;
; Returns:
;	0�`			(cy=0) �o�^�����p�^�[���ԍ�
;	InsufficientMemory	(cy=1) �������s��
;	GeneralFailure		(cy=1) tonum�� 512�ȏ�, fromnum�����o�^
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	186
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�����`���ł�OK
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
;	94/ 7/28 Initial: superdup.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 3/23 [M0.22k] to_num��512�ȏ�̂Ƃ��Ƀ������ɃS�~�c���Ă�

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

	EXTRN	HMEM_ALLOC:CALLMODEL

	.DATA
	EXTRN super_patsize:WORD
	EXTRN super_patdata:WORD
	EXTRN super_patnum:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN SUPER_ENTRY_AT:CALLMODEL

func SUPER_DUPLICATE	; super_duplicate() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	;������
	tonum	= (RETSIZE+2)*2
	fromnum	= (RETSIZE+1)*2

	mov	AX,[BP+tonum]	; 95/3/23�ǉ�
	cmp	AX,MAXPAT
	jae	short IGNORE

	mov	BX,[BP+fromnum]
	cmp	BX,super_patnum
	jae	short IGNORE
	shl	BX,1
	mov	DX,super_patsize[BX]
	test	DX,DX
	jz	short IGNORE

	mov	SI,super_patdata[BX]
	mov	AX,SI
	dec	AX
	mov	ES,AX
	mov	AX,word ptr ES:[2]
	sub	AX,SI			; �ړI�̃f�[�^�u���b�N�Ɠ����傫����
	mov	CX,AX
	push	AX
	mov	mem_AllocID,MEMID_super
	call	HMEM_ALLOC
	jc	short NOMEM
	push	DS
	mov	DS,SI
	mov	ES,AX
	xor	SI,SI
	shl	CX,3
	mov	DI,SI
	CLD
	rep	movsw
	pop	DS

	mov	DI,[BP+tonum]
	push	DI
	push	DX			; patsize
	push	ES			; seg
	_call	SUPER_ENTRY_AT
	jc	short DONE

	mov	AX,DI

DONE:
	pop	DI
	pop	SI
	pop	BP
	ret	4

IGNORE:
	mov	AX,GeneralFailure
	stc
	jmp	short DONE
NOMEM:
	mov	AX,InsufficientMemory
	jmp	short DONE
endfunc		; }

END
