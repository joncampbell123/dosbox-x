PAGE 98,120
; grcg - polygon - convex - Point
;
; DESCRIPTION:
;	�ʑ��p�`�`��(�v���[���̂�)
;
; FUNCTION:
;	void far _pascal grcg_polygon_c( Point far pts[], int npoint ) ;
;
; PARAMETERS:
;	Point pts[]	���W���X�g
;	int npoint	���W��( 2�` )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;	�E��L�́A���ׂ� draw_trapezoid �Ɉ˂�܂��B
;
; NOTES:
;	�E�F�w��́A�O���t�B�b�N�`���[�W���[���g�p���Ă��������B
;	�E�N���b�s���O�g�ɂ���ĕ`��𐧌��ł��܂��B( grc_setclip()�Q�� )
;	�E���E�̋��E�N���b�v�́A��`�`�惋�[�`���Ɉˑ����Ă��܂��B
;	�E�f�[�^�����L�̏����ɓ��Ă͂܂�ꍇ�A�������`�悵�܂���B
;	    ������y���W���o�R����ӂ�2�𒴂���ꏊ������ꍇ�B
;		�������ɂ��A�[�����Z������������A�������Ȑ}�`�ɂȂ�܂��B
;		�i�ʑ��p�`�łȂ��Ă��A��L�ȊO�Ȃ琳�����`�悵�܂��j
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; �֘A�֐�:
;	grc_setclip()
;	make_linework
;	draw_trapezoid
;
; AUTHOR:
;	���ˏ��F
;
; HISTORY:
;	92/5/18 Initial(C��)
;	92/5/19 bugfix
;	92/5/20 �Ƃ肠�����A���E�Ɖ��[�̃N���b�v��������B
;	92/5/21 ���E�̐攻��ƁA����N���b�v�������B�i���[�̂�т�B�j
;	92/6/4	Initial(asm�� gc_4corn.asm)
;	92/6/5	gc_polyg.asm
;	92/6/6	ClipYT_seg�Ή�
;	92/6/16 TASM�Ή�
;	92/6/19 gc_polgc.asm
;	92/6/21 Pascal�Ή�

	.MODEL SMALL
	.186

	; ��`
	EXTRN	make_linework:NEAR
	EXTRN	draw_trapezoid:NEAR
	EXTRN	trapez_a:WORD
	EXTRN	trapez_b:WORD

	; �N���b�v�g
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE

	include FUNC.INC

IF datasize EQ 2
MOVPTS	macro	to, from
	mov	to,ES:from
	endm
CMPPTS	macro	pts, dat
	cmp	ES:pts,dat
	endm
SUBPTS	macro	reg,pts
	sub	reg,ES:pts
	endm
LODPTS	macro	reg
	les	reg,[BP+pts]
	endm
ELSE
MOVPTS	macro	to, from
	mov	to,from
	endm
CMPPTS	macro	pts, dat
	cmp	pts,dat
	endm
SUBPTS	macro	reg,pts
	sub	reg,pts
	endm
LODPTS	macro	reg
	mov	reg,[BP+pts]
	endm
ENDIF

MRETURN macro
	pop	SI
	pop	DI
	leave
	ret	(datasize+1)*2
	EVEN
	endm

; void _pascal grcg_polygon_c( Point pts[], int npoint ) ;
;
func GRCG_POLYGON_C
	push	BP
	mov	BP,SP
	sub	SP,12	; ���[�J���ϐ��̃T�C�Y
	push	DI
	push	SI

	; �p�����[�^
	pts	= (RETSIZE+2)*2
	npoint	= (RETSIZE+1)*2

	; ���[�J���ϐ�
	ry = -2		; ����y
	ty = -4		; ��[��y
	ly = -6		; �E��y
	nl = -8		; ���̓_�̓Y����(byte�P��)
	nr = -10	; �E�̓_�̓Y����(byte�P��)
	by = -12	; ���[��y

	mov	AX,[BP+npoint]
	dec	AX
	mov	CX,AX			; CX = �{���� npoint
	shl	AX,2
	mov	[BP+npoint],AX		; npoint = (npoint-1)*4

	; �����T���` ----------------

	LODPTS	SI		; SI = pts
	xor	AX,AX
	mov	[BP+nl],AX	; nl = 0
	mov	[BP+nr],AX	; nr = 0
	MOVPTS	AX,[SI+2]
	mov	[BP+ty],AX	; ty = pts[0].y
	mov	[BP+by],AX	; by = pts[0].y
	MOVPTS	AX,[SI]		; AX = pts[0].x (AX=lx)
	mov	DI,AX		; DI = pts[0].x	(DI=rx)
	mov	BX,4		; BX = 1*4

LOOP1:	; x,y �̊e�ő�l�ƍŏ��l�𓾂郋�[�v
	MOVPTS	DX,[BX+SI+2]	; pts[i].y
	cmp	DX,[BP+ty]
	jge	short S10
		mov	[BP+nl],BX	; nl�́A����̍ŏ��ɂ݂����_
		mov	[BP+nr],BX	; nr�́A����̍Ō�Ɍ������_
		mov	[BP+ty],DX
		jmp	short S30
		EVEN
S10:
	cmp	[BP+ty],DX
	jne	short S20
		mov	[BP+nr],BX
		jmp	short S30
		EVEN
S20:
	cmp	[BP+by],DX
	jge	short S30
		mov	[BP+by],DX
S30:
	MOVPTS	DX,[BX+SI]	; pts[].x
	cmp	AX,DX
	jl	short S40
		mov	AX,DX
		jmp	short S50
		EVEN

		; �ւ�ȂƂ���ɐ����Ă邪�A�N���b�v�̋ߏ��return������
		; ����̂��`
	RETURN1:
		MRETURN
		EVEN
S40:	; else
		cmp	DI,DX
		jge	short S50
			mov	DI,DX
S50:
	add	BX,4
	loop	short LOOP1
	; ���[�v�I�� ----------------

	; �͈͌���
	cmp	ClipXR,AX
	jl	short RETURN1	; ���S�ɉE�ɊO��Ă�

	cmp	ClipXL,DI
	jg	short RETURN1	; ���S�ɍ��ɊO��Ă�

	mov	AX,[BP+ty]
	cmp	ClipYB,AX
	jl	short RETURN1	; ���S�ɉ��ɊO��Ă�

	mov	DX,[BP+by]
	cmp	ClipYT,DX
	jg	short RETURN1	; ���S�ɏ�ɊO��Ă�

	; **** �����A�����ɗ�������ɂ́A���Ȃ炸�`��ł���񂾂��B ****
	; (��`�ɂ��Ō�̃N���b�v�����Ăɂ��Ă邩��)

	; �`��Z�O�����g�ݒ�
IF datasize NE 2
	mov	ES,ClipYT_seg
ENDIF

	; ����y���W���ɃN���b�v�g�Őؒf����
	mov	AX,ClipYB
	cmp	AX,DX
	jge	short S100
		mov	DX,AX
S100:
	mov	[BP+by],DX

	; nl���ŏ��Anr�Ō�Ȃ�΁A���ꊷ���B�i�킩��ɂ����`�j
	; �i�ʑ��p�`�Ȃ�΁A����_�̑��ɓ���y���W�̓_�͂P�ȉ��ɂ���
	;�@�Ȃ�Ȃ��B�]���āA��������L����_�͂Q�ȓ��Ȃ̂ŁA���W���
	;�@�[���܂�����̂͂��̃p�^�[���݂̂ƂȂ�j
	mov	AX,[BP+npoint]
	cmp	WORD PTR [BP+nl],0
	jne	short S110
	cmp	[BP+nr],AX
	jne	short S110
		mov	[BP+nl],AX
		mov	WORD PTR [BP+nr],0
S110:

	; �����ӂ́A��ɂ͂ݏo�Ă��镔�����΂� ***
	mov	BX,[BP+nl]	; BX = nl
	mov	DX,[BP+npoint]
	mov	AX,ClipYT
LOOPL:
	mov	CX,BX
	sub	BX,4
	jns	short SKIPL
	mov	BX,DX
SKIPL:
	CMPPTS	[BX+SI+2],AX
	jl	short LOOPL
	MOVPTS	AX,[BX+SI+2]
	mov	[BP+ly],AX	; ly = pts[nl].y
	mov	[BP+nl],BX

	; �ŏ��̍����Ӄf�[�^���쐬

	mov	BX,CX		; BX = i
	MOVPTS	DX,[SI+BX+2]	; DX = ty = pts[i].y
	MOVPTS	DI,[SI+BX]	; DI = wx = pts[i].x
	mov	BX,[BP+nl]
	cmp	DX,ClipYT
	jge	short S200
		mov	AX,DX
		sub	AX,[BP+ly]	; push (ty-ly)
		PUSH	AX
		mov	AX,DI		; AX = ((DI=wx) - pts[nl].x)
		SUBPTS	AX,[SI+BX]
		mov	CX,ClipYT
		sub	CX,DX		; CX = (ClipYT - (DX=ty))
		imul	CX
		POP	CX
		idiv	CX		; DI = (wx-pts[nl].x) * (ClipYT-ty)
		add	DI,AX		;		 / (ty-ly) + wx
		mov	DX,ClipYT	; ty = ClipYT
S200:
	mov	[BP+ty],DX

	xchg	DI,DX
	MOVPTS	AX,[SI+BX]	; pts[nl].x
	mov	CX,[BP+ly]
	sub	CX,DI		; CX = (ly-ty)
	mov	BX,offset trapez_a
	call	make_linework


	; �E���ӂ́A��ɂ͂ݏo�Ă��镔�����΂� ***

	mov	BX,[BP+nr]	; BX = nr
	mov	CX,[BP+npoint]
	mov	AX,ClipYT
LOOPR:
	mov	DI,BX		; DI = i
	add	BX,4		; BX = nr
	cmp	BX,CX
	jle	short SKIPR
	xor	BX,BX
SKIPR:
	CMPPTS	[SI+BX+2],AX
	jl	short LOOPR

	MOVPTS	AX,[SI+BX+2]
	mov	[BP+ry],AX
	mov	[BP+nr],BX

	; �ŏ��̉E���Ӄf�[�^���쐬
	add	DI,SI
	MOVPTS	CX,[DI+2]	; CX = pts[i].y
	MOVPTS	DI,[DI]		; DI = wx = pts[i].x
	MOVPTS	BX,[SI+BX]	; BX = pts[nr].x

	cmp	CX,[BP+ty]
	jge	short S300
		mov	AX,ClipYT	; (ClipYT - pts[i].y)
		sub	AX,CX
		push	CX
		mov	CX,DI		; (wx - pts[nr].x)
		sub	CX,BX
		imul	CX
		pop	CX
		sub	CX,[BP+ry]	; (pts[i].y - ry)
		idiv	CX
		add	DI,AX		; + wx
S300:
	mov	DX,DI
	mov	AX,BX		; pts[nr].x
	mov	CX,[BP+ry]
	sub	CX,[BP+ty]
	mov	BX,offset trapez_b
	call	make_linework

; ���ۂɕ`�悵�Ă�����[��[��] ========================

	mov	SI,[BP+ty]	; SI = ty

DRAWLOOP:
	mov	DI,[BP+ly]	; DI = y2
	cmp	DI,[BP+ry]
	jle	short S400
	mov	DI,[BP+ry]
S400:
	cmp	[BP+by],DI	; ���N���b�v
	jle	short S500

	PUSH	DI
	lea	DX,[DI-1]
	sub	DX,SI		; DX = y2-1 - ty
	sub	SI,ClipYT
	mov	AX,SI		; SI = ty*80
	shl	SI,2
	add	SI,AX
	shl	SI,4
IF datasize EQ 2
	mov	ES,ClipYT_seg
ENDIF
	call	draw_trapezoid	; **** DRAW ****
	POP	SI

	LODPTS	DI		; DI = pts
	cmp	SI,[BP+ly]	; if ( ty == ly )
	jne	short S410

	mov	BX,[BP+nl]
	MOVPTS	DX,[BX+DI]	; DX = pts[nl].x
	sub	BX,4
	jns	short S405
	mov	BX,[BP+npoint]
S405:
	mov	[BP+nl],BX
	MOVPTS	AX,[DI+BX]	; AX = pts[nl].x
	MOVPTS	CX,[DI+BX+2]	; CX = (ly = pts[nl].y) - ty
	mov	[BP+ly],CX
	sub	CX,SI
	mov	BX,offset trapez_a
	; DX = last pts[nl].x
	call	make_linework

S410:				; if ( ty == ry )
	cmp	[BP+ry],SI
	jne	short DRAWLOOP

	mov	BX,[BP+nr]	; BX = nr
	MOVPTS	DX,[BX+DI]
	add	BX,4
	cmp	BX,[BP+npoint]
	jle	short S420
	xor	BX,BX
S420:
	mov	[BP+nr],BX
	MOVPTS	AX,[DI+BX]	; AX = pts[nr].x
	MOVPTS	CX,[DI+BX+2]	; CX = pts[nr].y
	mov	[BP+ry],CX	; ry = CX
	sub	CX,SI		; CX -= SI
	mov	BX,offset trapez_b
	push	offset DRAWLOOP
	jmp	make_linework		; �܂���!!
	EVEN

	; ��ԉ��̑�`
S500:

IF datasize EQ 2
	mov	ES,ClipYT_seg
ENDIF
	mov	DX,[BP+by]
	sub	DX,SI	; DX = by - ty
	sub	SI,ClipYT
	mov	AX,SI	; SI = ty*80
	shl	SI,2
	add	SI,AX
	shl	SI,4
	call	draw_trapezoid
	MRETURN
endfunc

END
