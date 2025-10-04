PAGE 98,120
; koichan library - graphics - grcg - trapezoid - PC98V
;
; Function:
;	void far _pascal grcg_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 ) ;
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
;	Microsoft-C / Turbo-C ( Small Model )
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
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
;	95/ 3/25 [M0.22k] BUGFIX ClipYT=0�����肵�Ă���

	.186

	.MODEL SMALL

	.DATA?
		EXTRN trapez_a:WORD, trapez_b:WORD	; ��`�`��p�ϐ�
		EXTRN ClipYT_seg:WORD			; VRAM��[�Z�O�����g
		EXTRN ClipYT:WORD			; ��ذ݂̏㉺�[��y��
		EXTRN ClipYH:WORD			; ��ذ݂̏㉺�[��y��

	.CODE
		include func.inc
		EXTRN make_linework:NEAR, draw_trapezoid:NEAR

;=========================================================================
; void _pascal grcg_trapezoid( int y1, int x11, int x12, int y2, int x21, int x22 )

FUNC GRCG_TRAPEZOID
	push	BP
	mov	BP,SP
	push	DI
	push	SI
	mov	ES,ClipYT_seg
	mov	AX,ClipYT
	mov	CX,ClipYH

	; ����
	y1  = (RETSIZE+6)*2
	x11 = (RETSIZE+5)*2
	x12 = (RETSIZE+4)*2
	y2  = (RETSIZE+3)*2
	x21 = (RETSIZE+2)*2
	x22 = (RETSIZE+1)*2

	mov	SI,[BP+y1]
	sub	SI,AX
	js	short Lno_draw
	mov	DI,[BP+y2]
	sub	DI,AX
	js	short Lno_draw

	cmp	SI,DI
	jg	short Lgyaku

	or	DI,DI
	js	short Lno_draw
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

	or	DI,DI
	js	short Lno_draw
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
	imul	SI,SI,80
	mov	DX,DI
	call	draw_trapezoid
Lno_draw:
	pop	SI
	pop	DI
	leave
	ret	12
ENDFUNC

END
