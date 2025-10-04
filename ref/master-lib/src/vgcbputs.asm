; master library module - VGA
;
; Description:
;	�O���t�B�b�N��ʂւ�BFNT������`��
;
; Functions/Procedures:
;	void vgc_bfnt_puts( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	�`��J�n������W
;	step	�������Ƃɐi�߂�h�b�g��(0=�i�߂Ȃ�)
;	anks	���p������
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	$Id: ankputs.asm 0.01 92/06/06 14:45:43 Kazumi Rel $
;
;	93/ 2/25 Initial: master.lib <- super.lib 0.22b
;	93/ 7/23 [M0.20] far������ɂ���ƑΉ�(^^;
;	                 ����graph_VramSeg�g���悤�ɂ���
;	94/ 1/24 Initial: grpbputs.asm/master.lib 0.22
;	94/ 4/11 Initial: vgcbputs.asm/master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

func VGC_BFNT_PUTS	; vgc_bfnt_puts() {
	enter	12,0
	push	SI
	push	DI
	_push	DS

	; ����
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	anks	= (RETSIZE+1)*2

	xlen	= -1
	ylen	= -2
	x_add	= -4
	patmul	= -6
	yadd	= -8
	patseg	= -10
	swidth  = -12

	mov	AX,font_AnkSeg
	mov	[BP+patseg],AX
	mov	ES,graph_VramSeg
	mov	AX,font_AnkSize
	mov	[BP+ylen],AX
	mov	AL,AH
	mov	AH,0
	sub	AX,graph_VramWidth
	not	AX
	mov	[BP+x_add],AX		; x_add = graph_VramWidth-xlen-1
	mov	AX,graph_VramWidth
	mov	[BP+swidth],AX
	mul	byte ptr [BP+ylen]
	mov	[BP+yadd],AX		; yadd = ylen * graph_VramWidth
	mov	AX,font_AnkPara
	mov	[BP+patmul],AX

	CLD
	_lds	SI,[BP+anks]

	; �ŏ��̕����͉�����
	lodsb
	or	AL,AL
	jz	short RETURN	; �����񂪋�Ȃ�Ȃɂ����Ȃ���

	mov	CX,[BP+x]
	mov	DI,[BP+y]

	mov	BX,AX
	mov	AX,[BP+swidth]
	mul	DI
	mov	DI,AX
	mov	AX,BX
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	EVEN
LOOPTOP:			; ������̃��[�v
	push	DS
	mul	byte ptr [BP+patmul]
	add	AX,[BP+patseg]
	mov	DS,AX		; ank seg
	xor	BX,BX

	; �����̕`��
	mov	DH,[BP+ylen]
DRAWY:
	mov	CH,[BP+xlen]
	mov	DL,0
DRAWX:
	mov	AH,0
	mov	AL,[BX]
	inc	BX
	ror	AX,CL
	or	AL,DL
	mov	DL,AH
	test	ES:[DI],AL
	stosb
	dec	CH
	jnz	short DRAWX
	mov	AL,AH
	test	ES:[DI],AL
	stosb

	add	DI,[BP+x_add]
	dec	DH
	jnz	short DRAWY

	sub	DI,[BP+yadd]
	add	CX,[BP+step]	;CH == 0!!
	mov	AX,CX
	and	CX,7
	sar	AX,3
	add	DI,AX

	pop	DS

	lodsb
	or	AL,AL
	jnz	short LOOPTOP

RETURN:

	_pop	DS
	pop	DI
	pop	SI
	leave
	ret	(3+DATASIZE)*2
endfunc			; }

END
