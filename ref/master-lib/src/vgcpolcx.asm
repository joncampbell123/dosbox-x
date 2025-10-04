; master library - polygon - convex - nonclip - VGA
;
; Description:
;	�ʑ��p�`�̕`�� �����i�@�\�팸�j��
;
; Funciton/Procedures:
;	void vgc_polygon_cx( const Point PTRPAT * pts, int npoint ) ;
;
; Parameters:
;	Point * pts	���W���X�g
;	int npoint	���W��
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA/SVGA 16Color
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�E�N���b�s���O�͈�؍s���Ă��܂���Bgrc_clip_polygon_n()�𗘗p����
;	�@�f�[�^�̃N���b�s���O���ɍs���Ă���Ă�ŉ������B
;	�E�f�[�^���ʑ��p�`�ɂȂ��Ă��Ȃ��ƁA�������`�悵�܂���B
;	�Enpoint �� 0 �ȉ��̏ꍇ�A�`�悵�܂���B
;	�Enpoint �� 1�`2 �̏ꍇ�Avgc_line���Ăяo���܂��B
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
;	92/ 7/11 Initial
;	92/ 7/18 �_�����Ȃ��Ƃ��̏���
;	93/ 6/28 2�_�ȉ��̂Ƃ��Alarge model�ł���������������
;	94/ 4/ 9 Initial: vgcpolcx.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	.DATA?

	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	.CODE
	include func.inc

; make_linework - DDA LINE�p�\���̂̍쐬
; IN:
;	DS:BX : LINEWORK * w ;	�������ݐ�
;	DX :	int x1 ;	��_��x
;	AX :	int x2 ;	���̓_��x
;	CX :	int y2 - y1 ;	�㉺��y�̍�
	EXTRN	make_linework:NEAR

; vgc_draw_trapezoidx - ��`�̕`�� �����i�@�\�팸�j��
; IN:
;	SI : unsigned yadr	; �`�悷��O�p�`�̏�̃��C���̍��[��VRAM�̾��
;	DX : unsigned ylen	; �`�悷��O�p�`�̏㉺�̃��C����(y2-y1)
;	ES : unsigned gseg	; �`�悷��VRAM�̃Z�O�����g(graph_VramSeg)
;	trapez_a		; ����a(�K����)��x�̏����l�ƌX��
;	trapez_b		; ����b(�K���E)��x�̏����l�ƌX��
	EXTRN	vgc_draw_trapezoidx:NEAR

	EXTRN	VGC_LINE:callmodel

IF datasize EQ 2
MOVPTS	macro	to, from
	mov	to,ES:from
	endm
PUSHPTS	macro	from
	push	ES:from
	endm
ESPTS	macro
	mov	ES,[BP+pts+2]
	endm
ESVRAM	macro
	mov	ES,graph_VramSeg
	endm
ELSE
MOVPTS	macro	to, from
	mov	to,from
	endm
PUSHPTS	macro	from
	push	from
	endm
ESPTS	macro
	endm
ESVRAM	macro
	endm
ENDIF
MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	(DATASIZE+1)*2
	EVEN
	endm

func VGC_POLYGON_CX	; vgc_polygon_cx() {
	enter	12,0
	push	SI
	push	DI

	; ����
	pts	= (RETSIZE+2)*2
	npoint	= (RETSIZE+1)*2

	; ���[�J���ϐ�
	nl = -10
	nr = -12
	ly = -4
	ry = -2
	ty = -6
	by = -8

if datasize eq 1
	mov	ES,graph_VramSeg
	mov	DI,[BP+pts]
else
	les	DI,[BP+pts]
endif

	; ���_��T���`
	mov	SI,[BP+npoint]	; SI=npoint-1
	dec	SI
	js	short RETURN
	mov	[BP+nr],SI
	mov	DX,SI		; DX:nl
	mov	AX,SI
	shl	AX,2
	add	DI,AX
	MOVPTS	BX,[DI+2]	; BX:ty
	mov	CX,BX		; CX:by
	dec	SI
	jg	short LOOP1	; ...LOOP1E

	; 1�`2�_�Ȃ璼��
	PUSHPTS	[DI]		; pts[npoint-1].x
	push	BX		; pts[npoint-1].y
	mov	DI,[BP+pts]
	PUSHPTS	[DI]		; pts[0].x
	PUSHPTS	[DI+2]		; pts[0].y
	call	VGC_LINE
RETURN:
	MRETURN

LOOP1:
	sub	DI,4
	MOVPTS	AX,[DI+2]
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
	mov	DI,[BP+pts]
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
	MOVPTS	CX,[BX+2]
	mov	[BP+ly],CX
	MOVPTS	AX,[SI+2]
	mov	[BP+ty],AX
	MOVPTS	DX,[SI]
	sub	CX,AX
	MOVPTS	AX,[BX]
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
	MOVPTS	AX,[BX+DI+2]
	mov	[BP+ry],AX
	mov	CX,BX
	mov	BX,DX
	shl	BX,2
	MOVPTS	DX,[BX+DI]
	mov	BX,CX

	; ���ۂɕ`�悵�Ă�����[��[��]
L4_010:
	sub	AX,[BP+ty]
	mov	CX,AX
	MOVPTS	AX,[BX+DI]
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
	jle	short LAST_ZOID
	push	SI
	push	DI
	lea	BX,[SI-1]
	xchg	SI,[BP+ty]
	sub	BX,SI
	xchg	AX,SI
	imul	graph_VramWidth
	xchg	SI,AX
	mov	DX,BX
	ESVRAM
	call	vgc_draw_trapezoidx		; ��ԉ��łȂ���`
	ESPTS
	pop	DI
	pop	SI

	cmp	SI,[BP+ly]
	jne	short L4_040

	; �����̑���
	mov	CX,[BP+nl]
	mov	BX,CX
	shl	BX,2
	MOVPTS	DX,[BX+DI]	; DX!
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
	MOVPTS	CX,[BX+2]
	mov	[BP+ly],CX
	sub	CX,[BP+ty]
	MOVPTS	AX,[BX]
	mov	BX,OFFSET trapez_a
	call	make_linework

L4_040:
	mov	AX,[BP+ry]
	cmp	[BP+ty],AX
	jne	short LOOP4

	; �E���̑���
	mov	SI,[BP+nr]
	mov	BX,SI
	shl	BX,2
	MOVPTS	DX,[BX+DI]	; DX!
	inc	SI
	cmp	SI,[BP+npoint]
	jl	short L4_050
	xor	SI,SI
L4_050:
	mov	BX,SI
	shl	BX,2
	MOVPTS	AX,[BX+DI+2]
	mov	[BP+ry],AX
	jmp	L4_010

LAST_ZOID:
	; ��ԉ��̑�`
	ESVRAM
	mov	AX,[BP+ty]
	mov	BX,[BP+by]
	sub	BX,AX
	imul	graph_VramWidth
	mov	SI,AX
	mov	DX,BX
	call	vgc_draw_trapezoidx
	MRETURN
endfunc			; }
END
