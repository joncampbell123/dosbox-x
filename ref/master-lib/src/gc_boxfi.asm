PAGE 98,120
; graphics - grcg - boxfill - PC98V
;
; Function:
;	void far _pascal grcg_boxfill( int x1, int y1, int x2, int y2 ) ;
;
; Description:
;	���h��(�v���[���̂�)
;
; Parameters:
;	int x1,y2	��P�_
;	int x2,y2	��Q�_
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
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;	for ALL memory model(SMALL�ȊO�ɂ���Ƃ��� .MODEL�����������ĉ�����)
;
; Notes:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O���s���Ă��܂��B
;
; �֘A�֐�:
;	grc_setclip()
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/6/10 Initial
;	92/6/10 ���ׂ�(�Q�̖���)�o�O���
;	92/6/16 TASM�Ή�

	.186
	.MODEL SMALL

	.DATA

	; grc_setclip()�֘A
	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	; �G�b�W�p�^�[��
	EXTRN	EDGES: WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

; void far _pascal grcg_boxfill( int x1, int y1, int x2, int y2 ) ;
retfunc RETURN
	MRETURN		; �Ё[�A����ȂƂ���ɁI�I
endfunc

func GRCG_BOXFILL
	push	BP
	push	SI
	push	DI

	CLI
	add	SP,(RETSIZE+3)*2
	pop	DI	; y2
	pop	SI	; x2
	pop	AX	; y1
	pop	BX	; x1
	sub	SP,(RETSIZE+3+4)*2
	STI

	; �܂��A�N���b�s���O����� ==========

	cmp	BX,SI		; BX <= SI �ɂ���B
	jle	short L1	;
	xchg	BX,SI		;
L1:				;
	mov	BP,ClipXL
	mov	DX,ClipXW
	sub	SI,BP		; SI < ClipXL �Ȃ�A�͈͊O
	jl	short RETURN	;
	sub	BX,BP
	cmp	BX,8000h	; BX < 0 �Ȃ�A BX = 0
	sbb	CX,CX		;
	and	BX,CX		;
	sub	SI,DX		; SI >= ClipXW �Ȃ�A SI = ClipXW
	sbb	CX,CX		;
	and	SI,CX		;
	add	SI,DX		;
	sub	SI,BX		; BX > SI �Ȃ�A�͈͊O
	jl	short RETURN	;
	add	BX,BP		;

	cmp	AX,DI		; y1 > y2 �Ȃ�΁A
	jle short L2		;   y1 <-> y2
	xchg	AX,DI		;
L2:				;
	mov	DX,ClipYT
	mov	BP,ClipYH
	sub	DI,DX		; y2 < ClipYT�Ȃ�Δ͈͊O
	js	short RETURN	;
	sub	AX,DX		; (y1-=ClipYT) < 0 �Ȃ�΁A
	cmp	AX,8000h	;
	sbb	CX,CX		;   y1 = 0
	and	AX,CX		;
	sub	DI,BP		; y2 >= YMAX �Ȃ�΁A
	sbb	CX,CX		;   y2 = YMAX
	and	DI,CX
	add	DI,BP
	sub	DI,AX		; y1 > y2�Ȃ�΁A
	jl	short RETURN	;   �͈͊O

	; BX = x1(left)
	; SI = abs(x2-x1)
	; AX = y1(upper) - ClipYT
	; DI = abs(y2-y1)

	; ���ɁA�f�[�^�쐬���Ċ����B =========

	mov	DX,AX		; ES = GramSeg + y1 * 5
	shl	AX,2		;
	add	AX,DX		;
	add	AX,ClipYT_seg	;
	mov	ES,AX		;

	mov	DX,DI		; DI = (y2-y1) * 80
	shl	DI,2		;
	add	DI,DX		;
	shl	DI,4		;
	mov	DX,BX		; DI += xl / 16 * 2
	shr	DX,4		;
	shl	DX,1		;
	add	DI,DX		;

	and	BX,0Fh
	add	SI,BX
	sub	SI,10h
	shl	BX,1
	mov	DX,[EDGES+BX]
	not	DX		; DX = first word
	mov	BX,SI
	and	BX,0Fh
	shl	BX,1
	mov	BX,[EDGES+2+BX]	; BX = last word

	sar	SI,4		; SI = num words
	js	short SHORTBOX

	; ���݂̃f�[�^ (AX,CX�̓t���[)
	; DX	first word
	; BX	last word
	; SI	num words
	; DI	start address
	; BP	sub offset
	; ES	gseg...

	; �Ō�A���h�肾���������� ===========

; �������Q���[�h�ȏ゠��Ƃ� ==============
	lea	BP,[2+40+SI]	; BP = (num words + 2 + 40) * 2
	shl	BP,1		;
	EVEN
YLOOP:
	mov	AX,DX
	stosw
	mov	AX,0FFFFh
	mov	CX,SI
	rep	stosw
	mov	AX,BX
	stosw
	sub	DI,BP
	jnb short YLOOP
	MRETURN
; �������P���[�h�����Ȃ��Ƃ� ==============
SHORTBOX:
	mov	BP,82		; BP = ����+1���[�h
	mov	AX,DX
	and	AX,BX
	EVEN
YLOOP2:
	stosw
	sub	DI,BP
	jnb short YLOOP2
	MRETURN
endfunc

END
