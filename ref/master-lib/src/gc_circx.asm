; master library - gc_poly - grcg - circle
;
; Description:
;	�~�`��(�N���b�s���O�Ȃ�)
;
; Function/Procedures:
;	void grcg_circle_x( int x, int y, int r ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	���a�� 0 �Ȃ�Ε`�悵�܂���B
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
;	93/ 5/12 Initial:gc_circl.asm/master.lib 0.16
;	93/ 5/14 ����
;	95/ 2/20 �c�{���[�h�Ή�

	.186
	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN	ClipYT:WORD
	EXTRN	ClipYT_seg:WORD
	EXTRN	graph_VramZoom:WORD

	.CODE

func GRCG_CIRCLE_X	; grcg_circle_x() {
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
	sub	AX,ClipYT
	mov	BX,[BP+x]

	mov	DS,ClipYT_seg

	mov	CS:_cx_,BX
	mov	CS:_cy_1,AX
	mov	CS:_cy_2,AX
	xor	AX,AX
	mov	BP,DX
	jmp	short LOOPSTART
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
	; DX == AX �̏ꍇ�� 2�x�`�悵�Ă邤
	; xor�`�悷��ƂW�̌ʂɂ���邼
LOOPSTART:
	push	BP
	JMOV	BP,_cx_

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

	mov	BX,SI
	shl	SI,2
	add	SI,BX
	shl	SI,4

	mov	BX,DI
	shl	DI,2
	add	DI,BX
	shl	DI,4

	mov	BX,BP
	sub	BX,DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	[BX+DI],CH
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,DX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	[BX+DI],CH
	mov	[BX+SI],CH

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

	mov	BX,SI
	shl	SI,2
	add	SI,BX
	shl	SI,4

	mov	BX,DI
	shl	DI,2
	add	DI,BX
	shl	DI,4

	mov	BX,BP
	sub	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	[BX+DI],CH
	mov	[BX+SI],CH

	mov	BX,BP
	add	BX,AX
	mov	CX,BX
	shr	BX,3
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	[BX+DI],CH
	mov	[BX+SI],CH
	pop	BP
	jmp	LOOPTOP
endfunc			; }

END
