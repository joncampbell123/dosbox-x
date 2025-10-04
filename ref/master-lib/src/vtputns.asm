; master library - vtext
;
; Description:
;	�e�L�X�g��ʂւ̕�����̏�������
;	���w�肠��E�����Ȃ�
;
; Function/Procedures:
;	void vtext_putns( unsigned x, unsigned y, char *strp, unsigned wid ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 255 )
;	unsigned y	��[�̍��W ( 0 �` 255 )
;	char * strp	������̐擪�A�h���X
;	unsigned wid	�\���̈�̌��� ( 0 �Ȃ�Ή������Ȃ� )
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
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
;	����
;
; Revision History:
;	93/10/06 Initial
;	93/10/08 puts_at() --> vtext_puts()
;	93/11/07 VTextState ����
;	94/ 4/ 9 Initial: vtputs.asm/master.lib 0.23
;	94/ 5/20 optimize, bugfix large data model
;	94/ 5/20 Initial: vtputns.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN Machine_State : WORD
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramWidth : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_PUTNS	; vtext_putns() {
	mov	ax,Machine_State
	test	ax,0010h	; PC/AT ����Ȃ��Ȃ�
	je	short EXIT

	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	strp	= (RETSIZE+2)*2
	wid	= (RETSIZE+1)*2

	CLD

	mov	CX,[BP+wid]
	jcxz	short EXIT2

	mov	DX,CX		; DX = copy of length of string

	mov	AX,TextVramWidth
	mul	byte ptr [BP+y]	; row
	add	AX,[BP+x]	; column
	add	AX,AX
	les	DI,TextVramAdr
	add	DI,AX

	_push	DS
	_lds	SI,[bp+strp]	; DS:SI = string address

	jmp	short SLOOP
	EVEN

POST_KANJI:	; �Ō�̊����̌�Ɉ�󔒂�����
		inc	CX
FILL_SPACE:	; �c�茅�ɋ󔒂��l�߂�
		mov	AL,' '	; space
SPACE_LOOP:
		mov	ES:[DI],AL
		add	DI,2
		loop	short SPACE_LOOP
		jmp	short DONE
	EVEN

SLOOP:	lodsb
	test	AL,AL
	jz	short FILL_SPACE
	cmp	AL,80h			; 81-9f e0-fd ?
	jbe	short ANK_OR_RIGHT
	cmp	AL,9fh
	jbe	short KANJI
	cmp	AL,0dfh
	jbe	short ANK_OR_RIGHT

KANJI:	dec	CX
	je	short POST_KANJI

	mov	ES:[DI],AL
	add	DI,2

	lodsb	; 2������: 40-7e,80-fc

ANK_OR_RIGHT:
	mov	ES:[DI],AL
	add	DI,2
	loop	short SLOOP

DONE:
	_pop	DS

	test	byte ptr VTextState,1
	jne	short NO_REF

	mov	CX,DX
	shl	DX,1
	sub	DI,DX
	mov	AH,0ffh		; Reflesh Screen
	int	10h

NO_REF:
EXIT2:
	pop	DI
	pop	SI
	pop	BP
EXIT:
	ret	(3+datasize)*2
endfunc		; }

END
