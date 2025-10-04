; gc_poly library - polygon - convex - nonclip - varargs
;
; Description:
;	�ʑ��p�`�̕`�� �����i�@�\�팸�j�A�ψ�����
;
; Funciton/Procedures:
;	void grcg_polygon_vcx( int npoint, int x1, in y1, ... ) ;
;
; Parameters:
;	int npoint	���W�_��
;	int x1,y1...	���W�f�[�^
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Notes:
;	�E�F�w��́A�O���t�B�b�N�`���[�W���[���g�p���Ă��������B
;	�E�N���b�s���O�͈�؍s���Ă��܂���Bgrc_clip_polygon_n()�𗘗p����
;	�@�f�[�^�̃N���b�s���O���ɍs���Ă���Ă�ŉ������B
;	�E�f�[�^���ʑ��p�`�ɂȂ��Ă��Ȃ��ƁA�������`�悵�܂���B
;	�Enpoint �� 0 �ȉ��̏ꍇ�A�`�悵�܂���B
;	�Enpoint �� 1�`2 �̏ꍇ�Agrcg_line���Ăяo���܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_clip_polygon_n()
;
; Revision History:
;	92/ 7/11 Initial: gc_polcx.asm
;	92/ 7/18 �_�����Ȃ��Ƃ��̏���
;	93/ 6/26 Initlal: gc_plvcx.asm/master.lib 0.19
;

	.186
	.MODEL SMALL
	.DATA?

	EXTRN	ClipYT_seg:WORD
	EXTRN	ClipYT:WORD

	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	.CODE
	include func.inc

	EXTRN	make_linework:NEAR
	EXTRN	draw_trapezoidx:NEAR
	EXTRN	GRCG_LINE:callmodel

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret
	EVEN
	endm

;---------------------------------
func _grcg_polygon_vcx	; grcg_polygon_vcx() {
	enter	12,0
	push	SI
	push	DI

	; ����
	npoint	= (RETSIZE+1)*2
	x1	= (RETSIZE+2)*2

	; ���[�J���ϐ�
	nl = -10
	nr = -12
	ly = -4
	ry = -2
	ty = -6
	by = -8

	mov	ES,ClipYT_seg
	lea	DI,[BP+x1]

	; ���_��T���`
	mov	SI,[BP+npoint]	; SI=npoint-1
	dec	SI
	js	short RETURN
	mov	[BP+nr],SI
	mov	DX,SI		; DX:nl
	mov	AX,SI
	shl	AX,2
	add	DI,AX
	mov	BX,SS:[DI+2]	; BX:ty
	mov	CX,BX		; CX:by
	dec	SI
	jg	short LOOP1	; ...LOOP1E

	; 1�`2�_�Ȃ璼��
	push	SS:[DI]		; pts[npoint-1].x
	push	BX		; pts[npoint-1].y
	push	[BP+x1]		; pts[0].x
	push	[BP+x1+2]	; pts[0].y
	call	GRCG_LINE
RETURN:
	MRETURN

LOOP1:
	sub	DI,4
	mov	AX,SS:[DI+2]
	cmp	AX,BX		; ty
	je	short L1_010
	jg	short L1_020
	mov	[BP+nr],SI
	mov	DX,SI		; nl
	mov	BX,AX		; ty
	jmp	short L1_030
L1_010:
	mov	DX,SI		; nl
	jmp	short L1_030
L1_020:
	cmp	CX,AX		; by
	jge	short L1_030
	mov	CX,AX		; by
L1_030:
	dec	SI
	jns	short LOOP1
LOOP1E:
	or	DX,DX		; nl
	jne	short L1_100
	mov	AX,[BP+npoint]
	dec	AX
	cmp	AX,[BP+nr]
	jne	short L1_100
	mov	DX,AX
	mov	WORD PTR [BP+nr],0
L1_100:
	mov	[BP+by],CX
	lea	DI,[BP+x1]
	; �ŏ��̍����Ӄf�[�^���쐬
	mov	BX,DX		; BX=nl
	dec	BX
	jge	short L2_010
	mov	BX,[BP+npoint]
	dec	BX
L2_010:
	mov	[BP+nl],BX
	shl	BX,2
	add	BX,DI
	mov	SI,DX
	shl	SI,2
	add	SI,DI
	mov	CX,SS:[BX+2]
	mov	[BP+ly],CX
	mov	AX,SS:[SI+2]
	mov	[BP+ty],AX
	mov	DX,SS:[SI]
	sub	CX,AX
	mov	AX,SS:[BX]
	mov	BX,offset trapez_a
	call	make_linework

	; �ŏ��̉E���Ӄf�[�^���쐬
	mov	SI,[BP+nr]
	mov	DX,SI		; DX
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L3_010
	xor	SI,SI
L3_010:
	mov	BX,SI
	mov	[BP+nr],SI
	shl	BX,2
	add	BX,DI
	mov	AX,SS:[BX+2]
	mov	[BP+ry],AX
	mov	CX,BX
	mov	BX,DX
	shl	BX,2
	mov	DX,SS:[BX+DI]
	mov	BX,CX

	; ���ۂɕ`�悵�Ă�����[��[��]
L4_010:
	sub	AX,[BP+ty]
	mov	CX,AX
	mov	AX,SS:[BX]
	mov	BX,offset trapez_b
	call	make_linework
	mov	[BP+nr],SI
LOOP4:
	mov	SI,[BP+ly]
	cmp	SI,[BP+ry]
	jle	short L4_020
	mov	SI,[BP+ry]
L4_020:
	cmp	[BP+by],SI
	;jg	short HOGE
	jle	short LAST_ZOID		; �Ȃ��
HOGE:
	push	SI
	push	DI
	lea	DX,[SI-1]
	xchg	SI,[BP+ty]
	sub	DX,SI
	sub	SI,ClipYT
	imul	SI,SI,80
	call	draw_trapezoidx		; ��ԉ��łȂ���`
	pop	DI
	pop	SI

	cmp	SI,[BP+ly]
	jne	short L4_040

	; �����̑���
	mov	CX,[BP+nl]
	mov	BX,CX
	shl	BX,2
	mov	DX,SS:[BX+DI]	; DX!
	dec	CX
	jns	short L4_030
	mov	CX,[BP+npoint]
	mov	BX,CX
	shl	BX,2
	dec	CX
L4_030:
	mov	[BP+nl],CX
	sub	BX,4
	add	BX,DI
	mov	CX,SS:[BX+2]
	mov	[BP+ly],CX
	sub	CX,[BP+ty]
	mov	AX,SS:[BX]
	mov	BX,offset trapez_a
	call	make_linework

L4_040:
	mov	AX,[BP+ry]
	cmp	[BP+ty],AX
	jne	short LOOP4

	; �E���̑���
	mov	SI,[BP+nr]
	mov	BX,SI
	shl	BX,2
	mov	DX,SS:[BX+DI]	; DX!
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L4_050
	xor	SI,SI
L4_050:
	mov	BX,SI
	shl	BX,2
	add	BX,DI
	mov	AX,SS:[BX+2]
	mov	[BP+ry],AX
	jmp	L4_010

LAST_ZOID:
	; ��ԉ��̑�`
	mov	SI,[BP+ty]
	mov	DX,[BP+by]
	sub	DX,SI
	sub	SI,ClipYT
	imul	SI,SI,80
	call	draw_trapezoidx
	MRETURN
endfunc		; }

END
