PAGE 98,120
; grcg - triangle - no_clipping
;
; Description:
;	�O�p�`�h��Ԃ�(�v���[���̂�,�N���b�s���O�Ȃ�)
;
; Functions/Procedures:
;	void grcg_triangle_x( Point * pts )
;
; Parameters:
;	Point * pts	�R�v�f�̓_�̔z��
;
; Binding Target:
;	Microsoft-C / Turbo-C / MS-PASCAL? / MS-FORTRAN?
;
; Running Target:
;	grcg_trapezoidx �Ɉˑ�
;
; Requiring Resources:
;	CPU: V30
;	grcg_trapezoidx �Ɉˑ�
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	(���炩����gc_color�ŐF��ݒ肵�Ă���Ăт����A�I������gc_reset()��
;	 �O���t�B�b�N�`���[�W���[�̃X�C�b�`��؂�)
;	�E�N���b�s���O�͍s���Ă��܂���B�N���b�v�g�O���w�肷��ƁA
;	�@����ʓ��ł��n���O�A�b�v����\����Ł[��
;	�@�@�K���f�[�^���N���b�v�g���ł��邱�Ƃ��ۏ؂��ꂽ��ԂŎ��s����
;	�@�������B
;
; �֘A�֐�:
;	grcg_setcolor(), grcg_off()
;	grc_setclip()
;	make_linework
;	draw_trapezoidx
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
;	93/3/2  �N���b�v�Ȃ���, ������ Point *�ɕύX

	.186
	.MODEL SMALL

	; ��`
	EXTRN make_linework:NEAR
	EXTRN draw_trapezoidx:NEAR
	EXTRN trapez_a:WORD
	EXTRN trapez_b:WORD

	; �N���b�v�g
	EXTRN ClipYT:WORD
	EXTRN ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	DATASIZE*2
	EVEN
	endm

;=========================================================================
; void grcg_triangle_x( Point * pts )
func GRCG_TRIANGLE_X	; {
	enter	10,0
	push	SI
	push	DI

	; ����
	pts = (RETSIZE+1)*2

	; �ϐ�
	x1   = -2
	x2   = -4
	y2   = -6
	x3   = -8
	y3   = -10

	_push	DS
	_lds	DI,[BP+pts]

	mov	AX,[DI]		; pts[0].x
	mov	BX,[DI+2]	; pts[0].y
	mov	CX,[DI+4]	; pts[1].x
	mov	DX,[DI+6]	; pts[1].y
	mov	SI,[DI+8]	; pts[2].x
	mov	DI,[DI+10]	; pts[2].y

	_pop	DS

	; �����W���ɕ��בւ�

	cmp	BX,DX		; y1,y2
	jle	short sort1
		xchg	AX,CX	; x1,x2
		xchg	BX,DX	; y1,y2
sort1:
	cmp	BX,DI		; y1,y3
	jl	short sort2
		xchg	AX,SI	; x1,x3
		xchg	BX,DI	; y1,y3
sort2:
	cmp	DX,DI		; y2,y3
	jle	short sort3
		xchg	CX,SI	; x2,x3
		xchg	DX,DI	; y2,y3
sort3:
	mov	[BP+x1],AX
	mov	[BP+x2],CX
	sub	DX,BX		; y2 -= y1
	mov	[BP+y2],DX
	mov	[BP+x3],SI
	sub	DI,BX		; y3 -= y1
	mov	[BP+y3],DI

	; �`��Z�O�����g��ݒ肷��
	sub	BX,ClipYT
	mov	AX,BX		; ES = ClipYT_seg + (y1-ClipYT) * 5
	shl	AX,2
	add	AX,BX
	add	AX,ClipYT_seg
	mov	ES,AX

	mov	AX,SI		; AX = x3
	mov	CX,DI		; CX = DI = y3
	mov	BX,offset trapez_b
	mov	DX,[BP+x1]
	call	make_linework

	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	mov	CX,[BP+y2]

	jcxz	short SUIHEI_1		; y2 == y1
	cmp	DI,CX			; y3 == y2
	je	short SUIHEI_2

	; �����ȕӂ��Ȃ��ꍇ
DOUBLE:
	mov	[BP+y3],DI

	mov	DI,CX			; DI = y2
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	call	make_linework
	lea	DX,[DI-1]
	xor	SI,SI
	call	draw_trapezoidx
	mov	CX,[BP+y3]
	mov	AX,[BP+x3]
	sub	CX,[BP+y2]
	mov	DI,CX
	mov	DX,[BP+x2]
	mov	BX,offset trapez_a
	call	make_linework
	mov	DX,DI
	call	draw_trapezoidx
	MRETURN

; ��ӂ������ȏꍇ
SUIHEI_1:
	mov	AX,[BP+x3]
	mov	CX,DI			; y3 - y2 (y2 = y1 = 0����������Ȃ�)
	call	make_linework
	EVEN
TAIL:
	xor	SI,SI
	mov	DX,[BP+y3]
	call	draw_trapezoidx
	MRETURN			; ����ȏ��ɂ����^�[����

; ��ӂ������ȏꍇ
SUIHEI_2:
	mov	AX,DX			; x2
	mov	DX,[BP+x1]
	push	offset TAIL
	jmp	make_linework

endfunc			; }
END
