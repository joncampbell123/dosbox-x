; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̃p�X�J��������̏�������
;	���w�肠��E�����Ȃ�
;
; Function/Procedures:
;	void text_putnp( unsigned x, unsigned y, char *pasp, unsigned wid ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	char * pasp	�p�X�J��������̐擪�A�h���X ( NULL�͋֎~ )
;	unsigned wid	�t�B�[���h��
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


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_PUTNP
	push	BP
	mov	BP,SP

	push	SI
	push	DI

	; ����
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	pasp	= (RETSIZE+2)*2
	wid	= (RETSIZE+1)*2

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

	mov	BP,[BP+wid]	; �t�B�[���h��

	mov	CH,0
	mov	CL,[SI]	; �ŏ��̃o�C�g������

	min2	CX,BP,DX
	jcxz	short EXIT
	sub	BP,CX		; BP = �c���(�󔒂𖄂߂�)��

	inc	SI

	mov	BX,0fedfh ; -2,-(20h + 1)
	mov	DX,9f80h

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
		cmp	AL,DH
		jnb	short SKIP
			cmp	AL,DL
			adc	AX,BX	; (stc)
SKIP:		sbb	AL,BH	; (BH = -2)

		and	AX,7f7fh
		xchg	AH,AL
		stosw
		or	AH,DL
ANK_OR_RIGHT:	stosw
	loop	short SLOOP

FILLSPACE:
	add	CX,BP
	mov	AX,' '	; SPACE
	rep	stosw

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
