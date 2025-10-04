; master library - PC/AT
;
; Description:
;	�e�L�X�g��ʂ̕����ۑ�/����
;
; Function/Procedures:
;	�ۑ�
;	void vtext_get( int x1,int y1, int x2,int y2, void far *buf ) ;
;
;	����
;	void vtext_put( int x1,int y1, int x2,int y2, const void far *buf ) ;
;
; Parameters:
;	int x1,x2 : 0�`
;	int y1,y2 : 0�`
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	vtext_get�͂�����҂񂮂��ĂȂ���
;	vtext_put�͉������N���b�s���O���Ă��
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 2/ 5 Initial
;	94/ 9/13 Initial: vtgetput.asm/master.lib 0.23
;	95/ 2/ 3 [M0.23] BUGFIX vtext_put ���S�������ĂȂ�����
;
	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramWidth : WORD
	EXTRN TextVramSize : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_GET	; vtext_get() {
	push	BP
	mov	BP,SP

	; ����
	x1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	x2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	CLD
	push	SI
	push	DI
	push	DS

	mov	SI,[BP+x1]	; ���� x1 > x2 �Ȃ�A
	mov	DX,[BP+x2]	;	x1 <-> x2
	cmp	SI,DX		;
	jle	short GET_SKIP1	;
	xchg	SI,DX		;
GET_SKIP1:			; SI = x1
	sub	DX,SI		; DX = x2 - SI + 1
	inc	DX		; �EDX�͉��̕�����

	mov	AX,[BP+y1]	; AX = y1
	mov	BX,[BP+y2]
	cmp	BX,AX
	jg	short GET_SKIP2
	xchg	AX,BX
GET_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;

	mov	CX,DX
	imul	TextVramWidth	; SI += y1 * TextVramWidth
	mov	DX,CX
	add	SI,AX
	add	SI,SI		; SI <<= 1

	mov	AX,TextVramWidth
	sub	AX,DX
	add	AX,AX		; AX = (TextVramWidth - width)*2

	lds	CX,TextVramAdr
	add	SI,CX
	les	DI,[BP+buf]

GET_LOOP:	; �c�̃��[�v
	mov	CX,DX
	rep movsw
	add	SI,AX		; ���̍s
	dec	BX
	jnz	short GET_LOOP

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc		; }

func VTEXT_PUT ; vtext_put() {
	push	BP
	mov	BP,SP

	; ����
	x1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	x2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	buf	= (RETSIZE+1)*2

	push	SI
	push	DI

	CLD
	mov	ES,word ptr TextVramAdr+2
	mov	SI,[BP+buf]

	mov	DI,[BP+x1]	; ���� x1 > x2 �Ȃ�A
	mov	DX,[BP+x2]	;	x1 <-> x2
	cmp	DI,DX		;
	jle	short PUT_SKIP1	;
	xchg	DI,DX		;
PUT_SKIP1:
	sub	DX,DI		; DI = x1
	inc	DX		; DX = x2 - DI + 1

	mov	CX,0		; CX�����������ɂȂ��[��
	mov	BX,DX		; BX = ��������
	mov	AX,DI		;
	cwd
	and	AX,DX
	mov	CX,AX		; CX = ���ɂ݂͂ł�������(����)
	sub	DI,CX		; DI = max(x1,0)
	sub	SI,CX		; SI += ���ɂ͂ݏo��������*2
	sub	SI,CX		; 
	lea	AX,[BX+DI]
	sub	AX,TextVramWidth ; AX=(��������+max(x1,0))-TextVramWidth
	cmc
	sbb	DX,DX
	and	AX,DX		; AX=�E�ɂ݂͂ł�������(����)
	sub	CX,AX		; CX=���E�ɂ͂ݏo���������̍��v(����)
	add	BX,CX		; BX=���ۂɕ`�悷�ׂ�������
	js	short CLIPOUT
	mov	DX,BX		; DX=���ۂɕ`�悷�ׂ�������
	shl	CX,1		; CX=���E�ɂ͂ݏo���o�C�g���̍��v(����)

	push	DS		; push DS

	push	CX		; �X�L�b�v�� -> stack

	mov	AX,[BP+y1]	;
	mov	BX,[BP+y2]	;
	cmp	AX,BX		;
	jle	short PUT_SKIP2	; ���� y1 > y2 �Ȃ�A
	xchg	AX,BX		;	y1 <-> y2
PUT_SKIP2:
	sub	BX,AX		; BX = y2 - y1 + 1
	inc	BX		;

	shl	DI,1
	add	DI,word ptr TextVramAdr
	mov	CX,TextVramWidth
	add	CX,CX
	mov	DS,[BP+buf+2]

	; AX  y1
	; BX  ylen
	; CX  TextVramWidth*2
	; DX  ���ۂɕ`�悷�ׂ�������
	; DI  TextVramAdr + y1*TextVramWidth + max(0,x1)

	push	DX
	mul	CX
	add	DI,AX		; DI += y1 * (TextVramWidth * 2)
	pop	AX		; AX=�`�悷�ׂ�������
	sub	CX,AX
	sub	CX,AX		; CX = TextVramWidth*2 - �`�悷�ׂ�������*2

	pop	DX		; stack -> �\�[�X�̃X�L�b�v��(����)

	mov	BP,CX
	push	BX
	push	DI
PUT_LOOP:
	mov	CX,AX
	rep movsw
	sub	SI,DX
	add	DI,BP

	dec	BX
	jne	short PUT_LOOP
	pop	DI
	pop	BX

	pop	DS		; pop DS

	; �X�V
	test	VTextState,1
	jnz	short NO_REFRESH
	test	VTextState,4000h
;	jz	short ALL_REFRESH

	mov	DX,TextVramWidth
	shl	DX,1
REFRESH_LOOP:
	mov	CX,AX
	push	AX
	mov	AH,0ffh		; Refresh Screen
	int	10h
	pop	AX
	add	DI,DX
	dec	BX
	jne	short REFRESH_LOOP
	jmp	short NO_REFRESH

ALL_REFRESH:
	les	DI,TextVramAdr
	mov	CX,TextVramSize
	mov	AH,0ffh
	int	10h
NO_REFRESH:
CLIPOUT:
	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc	; }

END
