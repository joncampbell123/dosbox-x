; master library - graphic - ank - puts - PascalString
;
; Description:
;	�O���t�B�b�N��ʂւ̔��p������`��[�p�X�J���������]
;
; Function/Procedures:
;	void graph_ank_putp( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	�`��J�n������W
;	step	�������Ƃɐi�߂�h�b�g��(0=�i�߂Ȃ�)
;	anks	���p������(�p�X�J��������)
;	color	�F(0�`15)
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
;	CPU: V30
;
; Notes:
;	
;
; Assembly Language Note:
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
;	93/ 7/23 Initial: grpaputp.asm/master.lib 0.20

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func GRAPH_ANK_PUTP ; graph_ank_putp() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; ����
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	anks	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	DX,font_AnkSeg
	mov	ES,graph_VramSeg

	_lds	SI,[BP+anks]

	; ������̒����͂����ق�?
	lodsb
	or	AL,AL
	jz	short RETURN	; �����񂪋�Ȃ�Ȃɂ����Ȃ���

	mov	AH,AL		; AH = length

	; �F�̐ݒ�
	mov	BX,[BP+color]
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	CX,4
GRCG_SET:
	shr	BX,1
	sbb	AL,AL
	out	7eh,AL
	loop	short GRCG_SET

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BL,[BP+step]

	mov	BP,DI		;-+
	shl	BP,2		; |
	add	DI,BP		; |DI=y*80
	shl	DI,4		;-+
	mov	BP,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BP,3		;AX=x/8
	add	DI,BP		;GVRAM offset address

	mov	CH,AH		; CH = length

	EVEN
LOOPTOP:			; ������̃��[�v
	lodsb
	push	DS
	xor	AH,AH
	add	AX,DX		; ank seg
	mov	DS,AX
	xor	BP,BP

	EVEN
ANK_LOOP:			; �����̕`��
	mov	AL,DS:[BP]
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,80 - 2
	inc	BP
	cmp	BP,16
	jnz	short ANK_LOOP

	sub	DI,80 * 16
	add	CL,BL
	mov	AL,CL
	xor	AH,AH
	and	CL,7
	shr	AX,3
	add	DI,AX

	pop	DS
	dec	CH
	jnz	short LOOPTOP

	xor	AL,AL
	out	7ch,AL		; GRCG OFF

RETURN:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(4+DATASIZE)*2
endfunc		; }

END
