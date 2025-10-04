; master library - VGA - trapezoid
;
; Function:
;	void vgc_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 ) ;
;
; Description:
;	��`�h��Ԃ�(�v���[���̂�)
;
; Parameters:
;	int y1			�P�ڂ̐������̂����W
;	int x11, x12		�P�ڂ̐������̗��[�̂����W
;	int y2			�Q�ڂ̐������̂����W
;	int x21, x22		�Q�ڂ̐������̗��[�̂����W
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16color
;
; Requiring Resources:
;	CPU: 186
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�E�N���b�s���O���s���Ă��܂��B
;	�@���E�����́Agrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂����A
;	�@�㉺�����́A��ʘg�ŃN���b�v���܂��B�Ăяo�����őΉ�����(˰)
;	�@�����W���㋫�E�ɂ������Ă���ƒx���Ȃ�܂��B
;	�@�i�㋫�E�Ƃ̌�_���v�Z�����ɁA�ォ�珇�ɒ��ׂĂ��邽�߁j
;	�E�ʒu�֌W�͏㉺�A���E�Ƃ����R�ł����A�˂���Ă���ꍇ
;	�@�˂��ꂽ��`�i�Q�̎O�p�`�����_�Őڂ��Ă����ԁj��`�悵�܂��B
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/3/21 Initial
;	92/3/29 bug fix, ��`�ɑ��ӂ̌�����F�߂�悤�ɂ���
;	92/4/2 ���X����
;	92/4/18 �O�p�`ٰ�݂��番���B�د��ݸޕt���B
;	92/4/19 ���X����
;	92/5/7 ���ȏ��������ɂ�����
;	92/5/20 �C�Ӹد��ݸނ̉��ɂ����Ή��B:-)
;	92/5/22 ����������ȏ��������`
;	92/6/6	bug fix
;	92/6/13 bug fix
;	92/6/16 TASM�Ή�
;	94/ 4/ 9 Initial: vgc_zoid.asm/master.lib 0.23

	.186
	.MODEL SMALL

	.DATA?
		EXTRN ClipYT:WORD, ClipYB:WORD
		EXTRN trapez_a:WORD, trapez_b:WORD	; ��`�`��p�ϐ�
		EXTRN graph_VramSeg:WORD		; VRAM��[�Z�O�����g
		EXTRN graph_VramWidth:WORD

	.CODE
		include func.inc
		EXTRN make_linework:NEAR, vgc_draw_trapezoid:NEAR

func VGC_TRAPEZOID	; vgc_trapezoid() {
	push	BP
	mov	BP,SP
	push	DI
	push	SI
	mov	ES,graph_VramSeg
	mov	CX,ClipYB

	; ����
	y1  = (RETSIZE+6)*2
	x11 = (RETSIZE+5)*2
	x12 = (RETSIZE+4)*2
	y2  = (RETSIZE+3)*2
	x21 = (RETSIZE+2)*2
	x22 = (RETSIZE+1)*2

	mov	SI,[BP+y1]
	mov	DI,[BP+y2]

	cmp	SI,DI
	jg	short Lgyaku

	cmp	DI,ClipYT
	jl	short Lno_draw
	cmp	SI,CX
	jg	short Lno_draw
	sub	DI,CX			; if ( y2 >= Ymax )
	sbb	AX,AX			;	y2 = Ymax ;
	and	DI,AX
	add	DI,CX
	sub	DI,SI			; DI = y2 - y1
	jl	short Lno_draw

	mov	AX,[BP+x21]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_a	;a
	mov	DX,[BP+x11]
	call	make_linework

	mov	AX,[BP+x22]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_b	;b
	mov	DX,[BP+x12]
	push	offset Ldraw
	jmp	make_linework

Lgyaku:
	xchg	SI,DI			; y2<->y1

	cmp	DI,ClipYT
	jl	short Lno_draw
	cmp	SI,CX
	jg	short Lno_draw
	sub	DI,CX			; if ( y2 >= Ymax )
	sbb	AX,AX			;	y2 = Ymax ;
	and	DI,AX
	add	DI,CX

	sub	DI,SI			; DI = y2 - y1
	jl	short Lno_draw

	mov	AX,[BP+x11]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_a	;a
	mov	DX,[BP+x21]
	call	make_linework

	mov	AX,[BP+x12]
	mov	CX,DI			; y2-y1
	mov	BX,offset trapez_b	;b
	mov	DX,[BP+x22]
	call	make_linework
Ldraw:
	mov	AX,SI
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,DI
	call	vgc_draw_trapezoid
Lno_draw:
	pop	SI
	pop	DI
	leave
	ret	12
endfunc			; }

END
