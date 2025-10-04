; master library - VGA - 16Color - triangle
;
; Function:
;	void vgc_triangle( int x1, int y1, int x2, int y2, int x3, int y3 ) ;
;
; Description:
;	�O�p�`�h��Ԃ�
;
; Parameters:
;	int x1,y1		��P�_�̍��W
;	int x2,y2		��Q�_�̍��W
;	int x3,y3		��R�_�̍��W
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; RUNNING TARGET:
;	vgc_trapezoid �Ɉˑ�
;
; REQUIRING RESOURCES:
;	CPU: 186
;	vgc_trapezoid �Ɉˑ�
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�E�N���b�s���O��vgc_trapezoid�Ɉ˂��Ă��܂��B
;
; �֘A�֐�:
;	gc_color(), gc_reset()
;	grc_setclip()
;	make_linework
;	vgc_draw_trapezoid
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/3/21 Initial
;	92/3/29 bug fix, ��`�ɑ��ӂ̌�����F�߂�悤�ɂ���
;	92/4/2 ���X����
;	92/6/4	�O�̑�`�ĂԂ悤�ɂ�����A�d�l�����������`�Ё[
;	92/6/5	���̂Œ������̂��B
;	92/6/6	graph_VramSeg�Ή� = �N���b�s���O�Ή�����
;	92/6/16 TASM�Ή�
;	94/ 4/11 Initial: vgctrian.asm/master.lib 0.23

	.186
	.MODEL SMALL

	.DATA?
	; ��`
	EXTRN	trapez_a: WORD
	EXTRN	trapez_b: WORD

	; �N���b�v�g
	EXTRN	ClipYT: WORD
	EXTRN	ClipYB: WORD
	EXTRN	graph_VramSeg: WORD
	EXTRN	graph_VramWidth: WORD


	; �O�p�`�`��p�ϐ�
ylen	dw	?

	.CODE
	include func.inc

	EXTRN	make_linework: NEAR
	EXTRN	vgc_draw_trapezoid: NEAR

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	12
	EVEN
	endm

func VGC_TRIANGLE	; vgc_triangle() {
	push	BP
	mov	BP,SP
	push	DI
	push	SI

	; �p�����[�^
	x1 = (RETSIZE+6)*2
	y1 = (RETSIZE+5)*2
	x2 = (RETSIZE+4)*2
	y2 = (RETSIZE+3)*2
	x3 = (RETSIZE+2)*2
	y3 = (RETSIZE+1)*2

	; �`��Z�O�����g��ݒ肷��
	mov	ES,graph_VramSeg

	; �����W���ɕ��בւ�(�Ƃ肠�����`)
	mov	AX,[BP+x1]
	mov	BX,[BP+y1]
	mov	CX,[BP+x2]
	mov	DX,[BP+y2]
	mov	SI,[BP+x3]
	mov	DI,[BP+y3]

	cmp	BX,DX		; y1,y2
	jl	short sort1
		xchg	AX,CX	; x1,x2
		xchg	BX,DX	; y1,y2
sort1:
	cmp	BX,DI		; y1,y3
	jl	short sort2
		xchg	AX,SI	; x1,x3
		xchg	BX,DI	; y1,y3
sort2:
	cmp	DX,DI		; y2,y3
	jl	short sort3
		xchg	CX,SI	; x2,x3
		xchg	DX,DI	; y2,y3
sort3:
	cmp	BX,ClipYB
	jg	short TAIL	; ���S�ɘg�̉� �� clip out
	cmp	DI,ClipYT
	jl	short TAIL	; ���S�ɘg�̏� �� clip out

	mov	[BP+x1],AX
	mov	[BP+y1],BX
	mov	[BP+x2],CX
	mov	[BP+y2],DX
	mov	[BP+x3],SI
	mov	[BP+y3],DI

	;
	; DI = y3
	; AX = x3
	mov	AX,SI
	; SI = y1
	mov	SI,BX

	mov	CX,DI			; y3
	mov	BX,offset trapez_b
	sub	CX,SI			; y1
	mov	ylen,CX
	mov	DX,[BP+x1]
	call	make_linework

	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	mov	CX,[BP+y2]

	cmp	CX,SI			; y2,y1
	jne	short CHECK2
; ��ӂ������ȏꍇ
	mov	AX,[BP+x3]
	sub	DI,CX			; y3 - y2
	mov	CX,DI			;
	call	make_linework
	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	mov	DX,ylen
	push	offset TAIL
	jmp	vgc_draw_trapezoid
	EVEN
TAIL:
	MRETURN			; ����ȏ��ɂ����^�[����

CHECK2:
	cmp	DI,CX			; y3,y2
	jne	short DOUBLE
; ��ӂ������ȏꍇ
	sub	CX,SI			; y1
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	mov	DX,ylen
	push	offset TAIL
	jmp	vgc_draw_trapezoid

	EVEN
; �����ȕӂ��Ȃ��ꍇ
DOUBLE:
	sub	CX,SI			; DI = y2 - y1
	mov	DI,CX			;
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	mov	AX,[BP+x2]
	cmp	[BP+x3],AX

	mov	AX,graph_VramWidth
	imul	SI
	mov	SI,AX
	lea	DX,[DI-1]
	call	vgc_draw_trapezoid
	mov	CX,[BP+y3]
	mov	AX,[BP+x3]
	sub	CX,[BP+y2]
	mov	ylen,CX
	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	call	make_linework
	mov	DX,ylen
	call	vgc_draw_trapezoid
	MRETURN
endfunc			; }
END
