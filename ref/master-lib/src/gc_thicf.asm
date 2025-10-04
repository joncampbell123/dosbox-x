PAGE 98,120
; graphics - grcg - thick_line - PC98V
;
; Function:
;	void far _pascal grcg_thick_line( int x1, int y1, int x2, int y2,
;					unsigned wid, unsigned hei ) ;
;
; Description:
;	�l�p���y���ɂ�钼��
;
; Parameters:
;	int x1,y2	��P�_(�y���̍���)
;	int x2,y2	��Q�_(�y���̍���)
;	unsigned wid	�y���̉��̑���
;	unsigned hei	�y���̏c�̑���
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
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
;	���̂܂܂őS���������f���ɑΉ��B
;
; Notes:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O���s���Ă��܂��B
;
; �֘A�֐�:
;	grc_setclip()
;	grcg_polygon_c()
;	grcg_line()
;	grcg_boxfill()
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/6/11 Initial
;	92/6/12 ���`���B/0����(����΂���)�B�������B�������B
;		����ɁA�P���Ȍ����~�X�B�������������B
;	92/6/16 TASM�Ή�


	.186
	.MODEL SMALL
	.CODE
	include func.inc

	EXTRN	GRCG_POLYGON_C:CALLMODEL
	EXTRN	GRCG_LINE:CALLMODEL
	EXTRN	GRCG_BOXFILL:CALLMODEL

	; parameters
	x1  = (RETSIZE+6)*2
	y1  = (RETSIZE+5)*2
	x2  = (RETSIZE+4)*2
	y2  = (RETSIZE+3)*2
	wid = (RETSIZE+2)*2
	hei = (RETSIZE+1)*2

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	12
	EVEN
	endm

retfunc DUMMY
	; �����܂��͐����Ȃ̂ŁA�����`�ɈϏ�
BOXFILL:
	cmp	SI,CX
	jge	short S400
	xchg	SI,CX
S400:
	cmp	DI,BX
	jge	short S500
	xchg	BX,DI
S500:
	push	CX
	push	BX
	add	SI,[BP+wid]
	push	SI
	add	DI,[BP+hei]
	push	DI
	call	GRCG_BOXFILL
RETURN:
	MRETURN
endfunc

; void far _pascal grcg_thick_line( int x1, int y1, int x2, int y2,
;				unsigned wid, unsigned hei ) ;

func GRCG_THICK_LINE
	enter	24,0
	push	SI
	push	DI

	; local variables
	pts = -24

	mov	CX,[BP+x1]
	mov	BX,[BP+y1]
	mov	SI,[BP+x2]
	mov	DI,[BP+y2]

	mov	AX,[BP+hei]	; AX = hei
	mov	DX,AX
	or	DX,[BP+wid]
	jne	short S100

	; �������c���Ƃ��� 0 �Ȃ̂ŁA�����ֈϏ�
	push	CX
	push	BX
	push	SI
LINE:
	push	DI
	call	GRCG_LINE
	jmp	RETURN
	EVEN

S100:
	cmp	SI,CX
	je	short BOXFILL
	cmp	DI,BX
	je	short BOXFILL

	or	AX,AX		; hei
	jnz	short POLYGON

	; �c�̑����� 0 �Ȃ̂ŁA�����ɂ��邩�ǂ������肷��
	sub	DI,BX	; y2 -= y1
	mov	AX,SI	; AX = x2 - x1
	sub	AX,CX
	cwd
	idiv	DI	; AX /= y2
	cwd
	xor	AX,DX
	sub	AX,DX	; AX = abs(AX)
	add	DI,BX	; y2 += y1

	cmp	AX,[BP+wid]
	jle	short POLYGON

	; ���ɑ������A�����ő�p����
	cmp	SI,CX
	jge	short S200
	xchg	CX,SI
	xchg	BX,DI
S200:
	push	CX
	push	SI
	mov	AX,[BP+wid]
	add	AX,DI
	push	AX
	jmp	SHORT LINE
	EVEN

POLYGON:
	cmp	DI,BX
	jge	short S300
	xchg	CX,SI
	xchg	BX,DI
S300:
	mov	[BP+pts+0],CX	; p0 = (x1,y1)
	mov	[BP+pts+2],BX

	cmp	SI,CX
	jle	short POLY2

	mov	AX,[BP+wid]
	add	CX,AX		; p1 = (x1+wid,y1)
	mov	[BP+pts+4],CX
	mov	[BP+pts+6],BX

	sub	CX,AX
	add	AX,SI
	mov	[BP+pts+8],AX	; p2 = (x2+wid,y2)
	mov	[BP+pts+10],DI

	mov	DX,[BP+hei]
	add	DI,DX
	mov	[BP+pts+12],AX	; p3 = (x2+wid,y2+hei)
	mov	[BP+pts+14],DI

	mov	[BP+pts+16],SI	; p4 = (x2,y2+hei)
	mov	[BP+pts+18],DI

	mov	[BP+pts+20],CX	; p5 = (x1,y1+hei)
	add	BX,DX
	mov	[BP+pts+22],BX
	jmp	SHORT GO_POLYGON
	EVEN

POLY2:
	mov	AX,[BP+wid]
	add	CX,AX		; p1 = (x1+wid,y1)
	mov	[BP+pts+4],CX
	mov	[BP+pts+6],BX

	mov	DX,[BP+hei]
	add	BX,DX
	mov	[BP+pts+8],CX	; p2 = (x1+wid,y1+hei)
	mov	[BP+pts+10],BX

	add	AX,SI		; p3 = (x2+wid,y2+hei)
	add	DX,DI
	mov	[BP+pts+12],AX
	mov	[BP+pts+14],DX

	mov	[BP+pts+16],SI	; p4 = (x2,y2+hei)
	mov	[BP+pts+18],DX

	mov	[BP+pts+20],SI	; p5 = (x2,y2)
	mov	[BP+pts+22],DI

GO_POLYGON:
IF datasize EQ 2
	push	SS
ENDIF
	lea	AX,[BP+pts]
	push	AX
	push	6
	call	GRCG_POLYGON_C
	MRETURN
endfunc

END
