; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̕�����̏�������
;	���w��E��������
;
; Function/Procedures:
;	void text_putnsa( unsigned x, unsigned y, char *strp, unsigned wid, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	char * strp	������̐擪�A�h���X ( NULL�͋֎~ )
;	unsigned wid	�\���̈�̌��� ( 0 �Ȃ�Ή������Ȃ� )
;	unsigned atrb	�\������
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
;	�������Ō�̌��ɔ����܂�����ꍇ�A���p�󔒂ɒu������܂��B
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
	.186

	.DATA
	EXTRN	TextVramSeg:WORD

	.CODE

	include func.inc


FUNC TEXT_PUTNSA
	mov	BX,BP	; save BP
	mov	BP,SP

	push	SI
	push	DI

	; ����
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	strp	= (RETSIZE+2)*2
	wid	= (RETSIZE+1)*2
	atrb	= (RETSIZE+0)*2

	mov	AX,[BP + y]	; �A�h���X�v�Z
	mov	DI,AX
	shl	AX,1
	shl	AX,1
	add	DI,AX
	shl	DI,1		; DI = y * 10
	add	DI,TextVramSeg
	mov	ES,DI
	mov	DI,[BP + x]
	shl	DI,1

	_push	DS
	_lds	SI,[BP+strp]

	mov	CX,[BP + wid]

	push	CX

	push	[BP + atrb]

	mov	BP,BX	; restore BP

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	DX,9f80h

	jcxz	short EXIT
	jmp	short SLOOP

POST_KANJI:	; �Ō�̊����̌�Ɉ�󔒂�����
		inc	CX
FILL_SPACE:	; �c�茅�ɋ󔒂��l�߂�
		mov	AX,' '	; space
		rep stosw
		jmp	short EXIT
	EVEN

SLOOP:		lodsb
		or	AL,AL
		je	short FILL_SPACE
		xor	AH,AH
		cmp	AL,DL	; 80h		81-9f e0-fd ?
		jbe	short ANK_OR_RIGHT
		cmp	AL,DH	; 9fh
		jbe	short KANJI
		cmp	AL,BL	; 0dfh
		jbe	short ANK_OR_RIGHT
	;	cmp	AL,0fdh
	;	jnb	short ANK_OR_RIGHT
KANJI:			dec	CX
			je	short POST_KANJI

			mov	AH,AL
			lodsb	; 2������: 40-7e,80-fc
			shl	AH,1
			cmp	AL,DH
			jnb	short SKIP
				cmp	AL,DL
				adc	AX,BX	; (stc)
			SKIP:
			sbb	AL,BH	; (BH = -2)

			and	AX,7f7fh
			xchg	AH,AL
			stosw
			or	AL,DL
ANK_OR_RIGHT:	stosw
CHECKLOOP:
	loop	short SLOOP

EXIT:
	pop	AX
	add	DI,2000h-2
	pop	CX
	STD
	rep	stosw
	CLD

	_pop	DS
	pop	DI
	pop	SI
	ret	(4+datasize)*2
ENDFUNC

END
