; master library - PC98
;
; Description:
;	�풓�p���b�g�̌���( respal_exist )
;	�풓�p���b�g�̍쐬( respal_create )
;
; Function/Procedures:
;	unsigned respal_exist( void ) ;
;	int respal_create( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	(respal_exist)	�풓�p���b�g������������A0�ȊO��Ԃ��܂��B
;			(�풓�p���b�g�̃Z�O�����g�A�h���X)
;	(respal_create)	�쐬�ł��Ȃ�(�������s��)�Ȃ�� 0
;			�쐬�����Ȃ� 1
;			���łɑ��݂����� 2
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
;	respal_create�́A�܂����������s���Ă��Ȃ��ꍇ�́A�������s���܂��B
;	�ǂ���̊֐��Ƃ��AResPalSeg�ϐ��ɏ풓�p���b�g�̃Z�O�����g�A�h���X��
;	�i�[���܂��B
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

	.MODEL SMALL
	include func.inc

_mcb_flg	equ	0
_mcb_owner	equ	1
_mcb_size	equ	3

	.DATA

	EXTRN ResPalSeg : WORD

ResPalID	db	'pal98 grb', 0
IDLEN EQU 10

	.CODE

func RESPAL_EXIST
	push	SI
	push	DI

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
			mov	DI,10h ; MCB�̎�
			mov	CX,IDLEN
			mov	SI,offset ResPalID
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
	mov	ResPalSeg,AX

	pop	DI
	pop	SI
	ret
	EVEN
endfunc


func RESPAL_CREATE
	push	SI
	push	DI

	call	RESPAL_EXIST
	or	AX,AX
	mov	AX,2
	jnz	short IGNORE
CREATE:
	mov	AX,5800h	; �A���P�[�V�����X�g���e�W�𓾂�
	int	21h
	mov	DX,AX		; �����X�g���e�W��ۑ�����

	mov	AX,5801h
	mov	BX,1		; �K�v�ŏ��̃A���P�[�V����
	int	21h
	mov	AH,48h		; ���������蓖��
	mov	BX,4 ; 64/16
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
		mov	BX,4 ; 64/16
		int	21h
	ALLOC_OK:
	mov	CX,AX
	mov	ResPalSeg,AX

	dec	CX			; MCB��owner������������
	mov	ES,CX			;
	mov	AX,-1			;
	mov	ES:[_mcb_owner],AX	;
	inc	CX
	mov	ES,CX
	cld
	xor	DI,DI			; ID�̏�������
	mov	SI,offset ResPalID	;
	mov	CX,IDLEN		;
	rep movsb
	xor	AX,AX
	stosw
	stosw
	stosw
	mov	CX,1

DAME:
	mov	AX,5801h	; �A���P�[�V�����X�g���e�W�̕��A
	mov	BX,DX		;
	int	21h		;
	mov	AX,CX

IGNORE:
	pop	DI
	pop	SI
	ret
endfunc

END
