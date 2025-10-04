; master library - VGA - 16color
;
; Description:
;	VGA 16color �~�`��(�N���b�s���O�Ȃ�)
;
; Function/Procedures:
;	void vgc_circle_x( int x, int y, int r ) ;
;
; Parameters:
;	x,y	���S���W
;	r	���a (1�ȏ�)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16color
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	���a�� 0 �Ȃ�Ε`�悵�܂���B
;
; Notes:
;	�E�F������ɂ́Avgc_setcolor()�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 5/12 Initial:gc_circl.asm/master.lib 0.16
;	93/ 5/14 ����
;	94/ 3/ 7 Initial for VGA(by Ara)
;	94/ 4/ 4 xor�̂��߂ɉ~�ʂ�8����1�����̏d���`��������悤�ɂ����B
;	94/ 5/21 ��640dot�ȊO�ɑΉ�(�蔲��)
;	95/ 2/20 �c�{���[�h�Ή�

	.186
	.MODEL SMALL

	.DATA

	EXTRN	graph_VramSeg:WORD, graph_VramWidth:WORD
	EXTRN	graph_VramZoom:WORD

	.CODE
	include func.inc
	include	vgc.inc

func VGC_CIRCLE_X	; vgc_circle_x() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	push	DS

	; ����
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	r	= (RETSIZE+1)*2

	mov	DX,[BP+r]	; DX = ���a
	; ���a�� 0 �Ȃ�`�悵�Ȃ�
	test	DX,DX
	jz	short RETURN

	mov	AL,byte ptr graph_VramZoom
	mov	CS:_VRAMSHIFT1,AL
	mov	CS:_VRAMSHIFT2,AL

	mov	AX,[BP+y]
	mov	BX,[BP+x]

	mov	CS:_cx_,BX
	mov	CS:_cy_1,AX
	mov	CS:_cy_2,AX
	mov	AX,graph_VramWidth
	mov	CS:MUL1,AX
	mov	CS:MUL2,AX
	mov	CS:MUL3,AX
	mov	CS:MUL4,AX
	mov	DS,graph_VramSeg
	xor	AX,AX
	mov	BP,DX
	jmp	short LOOPSTART
QUICKRETURN:
	pop	BP
	EVEN
RETURN:
	pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	6
	EVEN

LOOPTOP:
	stc		; BP -= AX*2+1;
	sbb	BP,AX	;
	sub	BP,AX	;
	jns	short LOOPCHECK
	dec	DX	; --DX;
	add	BP,DX	; BP += DX*2;
	add	BP,DX	;
LOOPCHECK:
	inc	AX	; ++AX;
	cmp	DX,AX
	jl	short RETURN
LOOPSTART:
	push	BP
NEXTDRAW:
	JMOV	BP,_cx_		; BP = ���S��x

	; in: DX = dx
	;     AX = dy
	mov	DI,AX
	shr	DI,9
	org	$-1
_VRAMSHIFT1	db	?

	JMOV	BX,_cy_1
	mov	SI,BX
	sub	SI,DI
	add	DI,BX

	imul	SI,SI,1234		; SI = (���S��y-dy) * 80
	org $-2
MUL1	dw	?
	imul	DI,DI,1234		; DI = (���S��y+dy) * 80
	org $-2
MUL2	dw	?

	mov	BX,BP
	sub	BX,DX	; BX = BP-DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,DX	; BX = BP+DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	cmp	AX,DX			; ������ 8����1�n�_��2�d�`��������
	je	short QUICKRETURN

	; in: AX = dx
	;     DX = dy
	mov	DI,DX
	shr	DI,9
	org	$-1
_VRAMSHIFT2	db	?

	JMOV	BX,_cy_2
	mov	SI,BX
	sub	SI,DI
	add	DI,BX

	imul	SI,SI,1234
	org $-2
MUL3	dw	?
	imul	DI,DI,1234
	org $-2
MUL4	dw	?

	mov	BX,BP
	sub	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	test	[BX+DI],AL
	mov	[BX+DI],CH
	test	[BX+SI],AL
	mov	[BX+SI],CH
	pop	BP
	jmp	LOOPTOP
endfunc			; }

END
