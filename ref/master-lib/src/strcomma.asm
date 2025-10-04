; master library
;
; Description:
;	�J���}��������������쐬����
;
; Function/Procedures:
;	int str_comma( char * buf, long value, unsigned buflen ) ;
;
; Parameters:
;	char * buf	�������ݐ�
;	char * value	���l
;	unsigned buflen	�i�[��̃o�C�g��(������+1)
;
; Returns:
;	1 = ����
;	0 = ���s(����������Ȃ�)
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	none
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	92/11/26 Initial
;	92/12/13 �R�����g�w�b�_������������
;	94/ 6/16 [M0.23] BUGFIX: ret�ł�sp�����l���Ԉ���Ă���
;	95/ 3/15 [M0.22k] BUGFIX buflen���������Ƃ��ď�������Ă��̂�1���߂���

	.MODEL SMALL
	include func.inc

	.CODE

	EXTRN ASM_LBDIV:NEAR
	; DX:AX = DX:AX / CL
	; CH    = DX:AX % CL

func STR_COMMA	; str_comma() {
	push	BP
	mov	BP,SP
	_push	DS
	push	SI
	push	DI

	; ����
	buf	= (RETSIZE+4)*2
	value	= (RETSIZE+2)*2
	buflen	= (RETSIZE+1)*2

	_lds	SI,[BP+buf]
	mov	DI,SI			; DI = buf
	mov	BX,[BP+buflen]
	cmp	BX,0
	jle	short OVERFLOW
	lea	SI,[SI+BX-1]		; SI = buf + buflen - 1
	mov	DX,[BP+value+2]
	mov	AX,[BP+value+0]
	or	DX,DX
	jns	short PLUS
	not	DX			; ����
	not	AX
	add	AX,1
	adc	DX,0
PLUS:
	xor	BX,BX
	mov	[SI],BL			; '\0'�𖖔��ɏ�������
	cmp	SI,DI
	je	short OVERFLOW

VLOOP:
	cmp	BX,3
	jne	short DIGIT
	mov	CH,','
	xor	BX,BX
	jmp	short WRITE
DIGIT:
	mov	CL,10
	call	ASM_LBDIV
	add	CH,'0'
	inc	BX
WRITE:
	dec	SI
	mov	[SI],CH			; 1������������
	cmp	SI,DI
	je	short OVERFLOW
	mov	CX,DX
	or	CX,AX
	jnz	short VLOOP
	jmp	short TOP
	EVEN

OVERFLOW:
	mov	AX,0		; OVERFLOW; return 0
	jmp	short RETURN
	EVEN

TOP:	test	WORD PTR [BP+value+2],8000h
	jz	short SPACE
	mov	AL,'-'		; �����Ȃ� '-'����������
	dec	SI
	mov	[SI],AL
SPACE:	mov	CX,SI
	sub	CX,DI
	mov	BX,DS
	mov	ES,BX
	mov	AL,' '
	rep	stosb

	mov	AX,1		; SUCCESS; return 1

RETURN:
	pop	DI
	pop	SI
	_pop	DS
	pop	BP
	ret	(3+DATASIZE)*2
endfunc		; }

END
