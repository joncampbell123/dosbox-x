; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̃p�X�J��������̏�������
;	���w��Ȃ��E��������
;
; Function/Procedures:
;	void text_putp( unsigned x, unsigned y, char *pasp, unsigned atrb ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	char * pasp	�p�X�J��������̐擪�A�h���X ( NULL�͋֎~ )
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
;	�Ō�̕����������P�o�C�g�ڂ̏ꍇ�A���̂܂܂̃R�[�h���������݂܂��B
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

func TEXT_PUTPA
	push	BP
	mov	BP,SP

	; ����
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	pasp	= (RETSIZE+2)*2
	atrb	= (RETSIZE+1)*2

	push	SI
	push	DI
	_push	DS

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

	_lds	SI,[BP+pasp]	; ������
	mov	CH,0
	mov	CL,[SI]		; �ŏ��̃o�C�g������
	jcxz	short EXIT
	inc	SI

	mov	DX,CX		; ������ۑ�

	mov	BP,[BP+atrb]

	mov	BX,0fedfh ; -2,-(20h + 1)

SLOOP:		lodsb
		xor	AH,AH
		test	AL,0e0h
		jns	short ANK_OR_RIGHT	; 00�`7f = ANK
		jpe	short ANK_OR_RIGHT	; 80�`9f, e0�`ff = ����
						; (�ق�Ƃ� 81�`9f, e0�`fd��)
		cmp	CX,1
		je	short ANK_OR_RIGHT
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

	; ������������������
	mov	AX,BP
	mov	CX,DX
	shl	DX,1
	sub	DI,DX
	add	DI,2000h	; �����A�h���X�v�Z
	rep	stosw

EXIT:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
