; master library - MS-DOS
;
; Description:
;	���ϐ��̎擾
;
; Function/Procedures:
;	const char far * dos_getenv( unsigned envseg, const char * envname ) ;
;
; Parameters:
;	unsigned envseg	���ϐ��̈�̐擪�̃Z�O�����g�A�h���X
;			0�Ȃ�Ύ����̊��ϐ�
;	char * envname	���ϐ���(�p���͑啶���ɂ��邱��)
;
; Returns:
;	char far *	�����������ϐ��̓��e������ւ̃|�C���^
;			������Ȃ���� NULL
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
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/26 Initial
;	93/ 5/30 [M0.18] envseg = 0�Ȃ�Ύ����̊��ϐ�������悤�ɂ���

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETENV
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ����
	envseg	= (RETSIZE+1+DATASIZE)*2
	envname	= (RETSIZE+1)*2

	_lds	DI,[BP+envname]	; DS = FP_SEG(envname)
	mov	DX,DI		; DX = FP_OFF(envname)
	mov	BX,DS
	mov	ES,BX

	mov	CX,-1		; �w����ϐ����̒����𓾂�
	xor	AX,AX
	repne	scasb
	not	CX
	dec	CX

	mov	BX,[BP+envseg]
	test	BX,BX
	jnz	short GO
	mov	AH,62h		; get PSP
	int	21h
	mov	ES,BX
	mov	BX,ES:[2ch]
	xor	AX,AX
GO:
	mov	ES,BX
	mov	DI,AX		; 0
	mov	BX,CX		; BX = ����

	cmp	ES:[DI],AL
	je	short NOTFOUND
	EVEN
SLOOP:
	mov	AL,'='		; ���ϐ��̖��O�̒��������o��
	mov	CX,-1
	repne	scasb
	jne	short NOTFOUND	; house keeping
	not	CX
	dec	CX
	cmp	CX,BX		; ��������v���Ȃ���Ύ��փX�L�b�v
	jne	short NEXT

	mov	SI,DX		; ���O�̔�r
	sub	DI,CX
	dec	DI
	repe	cmpsb
	je	short FOUND

NEXT:	; ���̊��ϐ�����T��
	xor	AX,AX
	mov	CX,-1
	repne	scasb
	jne	short NOTFOUND	; house keeping
	cmp	ES:[DI],AL
	jne	short SLOOP

NOTFOUND:			; ������Ȃ�
	xor	AX,AX
	mov	DX,AX
	jmp	short RETURN
	EVEN

FOUND:				; ��������
	mov	DX,ES
	mov	AX,DI
	inc	AX

RETURN:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
