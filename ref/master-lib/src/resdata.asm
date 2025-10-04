; master library - MS-DOS
;
; Description:
;	�풓�f�[�^�̌���( resdata_exist )
;	�풓�f�[�^�̍쐬( resdata_create )
;
; Function/Procedures:
;	unsigned resdata_exist( char * idstr, unsigned idlen, unsigned parasize) ;
;	unsigned resdata_create( char * idstr, unsigned idlen, unsigned parasize) ;
;
; Parameters:
;	char * idstr ;		/* ���ʃf�[�^ */
;	unsigned idlen ;	/* ���ʃf�[�^�̒��� */
;	unsigned parasize ;	/* �f�[�^�u���b�N�̃p���O���t�T�C�Y */
;
; Returns:
;	(resdata_exist)		�풓�f�[�^������������A0�ȊO��Ԃ��܂��B
;				(�풓�f�[�^�̃Z�O�����g�A�h���X)
;	(resdata_create)	�쐬�ł��Ȃ�(�������s��)�Ȃ�� 0
;				�쐬�����Ȃ� (�풓�f�[�^�̃Z�O�����g�A�h���X)
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
;	�J���́Ados_free�ł���Ă�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial
;	92/11/21 for Pascal
;	93/ 4/15 respal_create�ɖ߂�l������
;	95/ 1/ 7 Initial: resdata.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

_mcb_flg	equ	0
_mcb_owner	equ	1
_mcb_size	equ	3

	.DATA

ResPalID	db	'pal98 grb', 0
IDLEN EQU 10

	.CODE

func RESDATA_EXIST	; resdata_exist() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ������
	id_str	 = (RETSIZE+3)*2
	id_len	 = (RETSIZE+2)*2
	parasize = (RETSIZE+1)*2

	; �풓�p���b�g�̑{��
	mov	AH, 52h
	int	21h
	CLD
	mov	BX, ES:[BX-2]
FIND:
	mov	ES,BX
	inc	BX
	mov	AX,ES:[_mcb_owner]
	or	AX,AX
	je	short SKIP
	mov	AX,ES:[_mcb_size]
	cmp	AX,[BP+parasize]
	jne	short SKIP
		mov	CX,[BP+id_len]
		_lds	SI,[BP+id_str]
		mov	DI,10h ; MCB�̎�
		rep cmpsb
		je	short FOUND
	SKIP:
	mov	AX,ES:[_mcb_size]
	add	BX,AX
	mov	AL,ES:[_mcb_flg]
	cmp	AL,'M'
	je	short FIND
NOTFOUND:
	mov	BX,0
FOUND:
	mov	AX,BX

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc			; }


func RESDATA_CREATE	; resdata_create() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ������
	id_str	 = (RETSIZE+3)*2
	id_len	 = (RETSIZE+2)*2
	parasize = (RETSIZE+1)*2

	_push	[BP+id_str+2]
	push	[BP+id_str]
	push	[BP+id_len]
	push	[BP+parasize]
	_call	RESDATA_EXIST
	or	AX,AX
	jnz	short IGNORE
CREATE:
	mov	AX,5800h	; �A���P�[�V�����X�g���e�W�𓾂�
	int	21h
	mov	DX,AX		; �����X�g���e�W��ۑ�����

	mov	AX,5801h
	mov	BX,1		; �K�v�ŏ��̃A���P�[�V����
	int	21h
	mov	AH,48h		; ���������蓖��
	mov	BX,[BP+parasize]
	int	21h
	mov	CX,0
	jc	short DAME
	mov	BX,CS		; �������O�Ȃ�n�j
	cmp	BX,AX
	jnb	short ALLOC_OK
		mov	ES,AX		; ��������낾������
		mov	AH,49h		; �������B
		int	21h		;

		mov	AX,5801h	;
		mov	BX,2		; �ŏ�ʂ���̃A���P�[�V����
		int	21h

		mov	AH,48h		; ���������蓖��
		mov	BX,[BP+parasize]
		int	21h
	ALLOC_OK:
	mov	CX,AX
	push	AX

	dec	CX			; MCB��owner������������
	mov	ES,CX			;
	mov	AX,-1			;
	mov	ES:[_mcb_owner],AX	;
	inc	CX
	mov	ES,CX
	xor	DI,DI			; ID�̏�������
	mov	CX,[BP+id_len]
	_lds	SI,[BP+id_str]
	rep movsb

	pop	CX

DAME:
	mov	AX,5801h	; �A���P�[�V�����X�g���e�W�̕��A
	mov	BX,DX		;
	int	21h		;
	mov	AX,CX

IGNORE:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc			; }

END
