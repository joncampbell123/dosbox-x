PAGE 98,120
; grcg - triangle
;
; Function:
;	void far _pascal grcg_triangle( int x1, int y1, int x2, int y2, int x3, int y3 ) ;
;
; Description:
;	�O�p�`�h��Ԃ�(�v���[���̂�)
;
; Parameters:
;	int x1,y1		��P�_�̍��W
;	int x2,y2		��Q�_�̍��W
;	int x3,y3		��R�_�̍��W
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C / MS-PASCAL? / MS-FORTRAN?
;
; RUNNING TARGET:
;	grcg_trapezoid �Ɉˑ�
;
; REQUIRING RESOURCES:
;	CPU: V30
;	grcg_trapezoid �Ɉˑ�
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	(���炩����gc_color�ŐF��ݒ肵�Ă���Ăт����A�I������gc_reset()��
;	 �O���t�B�b�N�`���[�W���[�̃X�C�b�`��؂�)
;	�E�N���b�s���O��grcg_trapezoid�Ɉ˂��Ă��܂��B
;
; �֘A�֐�:
;	gc_color(), gc_reset()
;	grc_setclip()
;	make_linework
;	draw_trapezoid
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
;	92/6/6	ClipYT_seg�Ή� = �N���b�s���O�Ή�����
;	92/6/16 TASM�Ή�

	.186

	.MODEL SMALL

	; ��`
	EXTRN	make_linework: near
	EXTRN	draw_trapezoid: near
	EXTRN	trapez_a: word
	EXTRN	trapez_b: word

	; �N���b�v�g
	EXTRN	ClipYT: word
	EXTRN	ClipYH: word
	EXTRN	ClipYT_seg: word

	.DATA?

	; �O�p�`�`��p�ϐ�
ylen	dw	?

	.CODE
	include func.inc

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	12
	EVEN
	endm

;=========================================================================
; void far _pascal grcg_triangle( int x1,int y1, int x2,int y2, int x3,int y3 )
func GRCG_TRIANGLE
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
	mov	ES,ClipYT_seg

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
	mov	[BP+x1],AX
	mov	AX,ClipYT
	sub	BX,AX
	cmp	BX,ClipYH
	jg	short TAIL	; ���S�ɘg�̉� �� clip out
	mov	[BP+y1],BX
	mov	[BP+x2],CX
	sub	DX,AX
	mov	[BP+y2],DX
	mov	[BP+x3],SI
	sub	DI,AX
	jl	short TAIL	; ���S�ɘg�̏� �� clip out
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
	imul	SI,SI,80
	mov	DX,ylen
	push	offset TAIL
	jmp	draw_trapezoid
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
	imul	SI,SI,80
	mov	DX,ylen
	push	offset TAIL
	jmp	draw_trapezoid

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

	lea	DX,[DI-1]
	imul	SI,SI,80
	call	draw_trapezoid
	mov	CX,[BP+y3]
	mov	AX,[BP+x3]
	sub	CX,[BP+y2]
	mov	ylen,CX
	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	call	make_linework
	mov	DX,ylen
	call	draw_trapezoid
	MRETURN

endfunc
END
