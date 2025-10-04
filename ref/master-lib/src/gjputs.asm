; master library - PC98
;
; Description:
;	�e�L�X�g��ʂւ̊O��������̏�������
;	���w��Ȃ��E�����Ȃ�
;
; Function/Procedures:
;	void gaiji_puts( unsigned x, unsigned y, char *strp ) ;
;
; Parameters:
;	unsigned x	���[�̍��W ( 0 �` 79 )
;	unsigned y	��[�̍��W ( 0 �` 24 )
;	char * strp	�O��������̐擪�A�h���X ( NULL�͋֎~ )
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
;	92/11/24 Initial
;	93/1/26 bugfix

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func GAIJI_PUTS
	mov	DX,BP	; push BP
	mov	BP,SP

	push	SI
	push	DI

	; ����
	x	= (RETSIZE+1+DATASIZE)*2
	y	= (RETSIZE+0+DATASIZE)*2
	strp	= (RETSIZE+0)*2

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

	mov	BP,DX	; pop BP

	lodsb
	or	AL,AL
	je	short EXITLOOP
	EVEN
SLOOP:
	mov	AH,AL
	mov	AL,0
	rol	AX,1
	shr	AX,1
	adc	AL,56h
	stosw
	or	AH,80h
	stosw
	lodsb
	or	AL,AL
	jne	short SLOOP
EXITLOOP:

	_pop	DS
	pop	DI
	pop	SI
	ret	(2+datasize)*2
endfunc

END
