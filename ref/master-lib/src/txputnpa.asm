; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̃p�X�J��������̏�������
;	���w�肠��E��������
;
; Function/Procedures:
;	void text_putnpa( unsigned x, unsigned y, char *pasp, unsigned wid, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	char * pasp	�p�X�J��������̐擪�A�h���X ( NULL�͋֎~ )
;	unsigned wid	�t�B�[���h��
;	unsigned atrb	��������
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
;	�Ō�̕����������P�o�C�g�ڂ̏ꍇ�A���p�󔒂ɂȂ�܂��B
;	�����̔���͌����ł͂���܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/12/18 Initial
;	95/ 3/23 [M0.22k] BUGFIX �����𕶎���̈�ɓh��ׂ��Ă��܂��Ă���


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PUTNPA	; text_putnpa() {
	push	BP
	mov	BP,SP

	push	SI
	push	DI

	; ����
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	pasp	= (RETSIZE+3)*2
	wid	= (RETSIZE+2)*2
	atrb	= (RETSIZE+1)*2

	CLD
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
	_lds	SI,[BP+pasp]

	mov	DX,[BP+wid]	; �t�B�[���h��
	mov	BX,[BP+atrb]	; ����

	mov	CH,0
	mov	CL,[SI]		; �ŏ��̃o�C�g������
	mov	BP,DX
	min2	CX,BP,AX
	jz	short EXIT
	sub	BP,CX		; BP = �c���(�󔒂𖄂߂�)��

	push	BX		; ����

	inc	SI

	mov	BX,0fedfh ; -2,-(20h + 1)

SLOOP:		lodsb
		xor	AH,AH
		test	AL,0e0h
		jns	short ANK_OR_RIGHT	; 00�`7f = ANK
		jp	short ANK_OR_RIGHT	; 80�`9f, e0�`ff = ����
						; (�ق�Ƃ� 81�`9f, e0�`fd��)
		cmp	CX,1
		je	short FILLSPACE
		dec	CX

		mov	AH,AL
		lodsb				; 2������: 40-7e,80-fc
		shl	AH,1
		cmp	AL,9fh
		jnb	short SKIP
			cmp	AL,80h
			adc	AX,BX	; (stc)
SKIP:		sbb	AL,BH	; (BH = -2)

		and	AX,7f7fh
		xchg	AH,AL
		stosw
		or	AH,80h
ANK_OR_RIGHT:	stosw
	loop	short SLOOP

FILLSPACE:
	add	CX,BP
	mov	AX,' '	; SPACE
	rep	stosw

	pop	AX	; ����
	mov	CX,DX
	add	DI,2000h
	shl	DX,1
	sub	DI,DX
	rep	stosw

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(4+DATASIZE)*2
endfunc			; }

END
