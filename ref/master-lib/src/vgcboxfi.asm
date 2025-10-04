; master library - VGA - 16color - boxfill
;
; Description:
;	VGA 16�F ���h��
;
; Functions/Procedures:
;	void vgc_boxfill( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	int x1,y2	��P�_
;	int x2,y2	��Q�_
;
; Returns:
;	none
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
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O���s���Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; �֘A�֐�:
;	grc_setclip(), vgc_setcolor()
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/12/ 3 Initial: vgcboxfi.asm/master.lib 0.22
;	94/ 4/ 8 [M0.23] ��640dot�ȊO�ɂ��Ή�

	.186
	.MODEL SMALL

	.DATA

	; grc_setclip()�֘A
	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYH:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	; �G�b�W�p�^�[��
	EXTRN	EDGES: WORD

	.CODE
	include func.inc
	include vgc.inc

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

retfunc RETURN
	MRETURN		; �Ё[�A����ȂƂ���ɁI�I
endfunc

func VGC_BOXFILL	; vgc_boxfill() {
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
	and	CX,DI		; CX=DI & CX
	add	CX,BP
	sub	CX,AX		; y1 > y2�Ȃ�΁A
	jl	short RETURN	;   �͈͊O

	inc	CX
	add	AX,DX

	; BX = x1(left)
	; SI = abs(x2-x1)
	; AX = y1(upper)
	; CX = abs(y2-y1)+1

	; ���ɁA�f�[�^�쐬���Ċ����B =========

	mov	ES,graph_VramSeg ; ES = graph_VramSeg

	mov	DI,BX		; DI += xl / 8
	shr	DI,3
	imul	graph_VramWidth
	add	DI,AX

	and	BX,07h
	lea	SI,[SI+BX-8]
	shl	BX,1
	mov	AL,byte ptr EDGES[BX]
	not	AX		; AH = first byte
	mov	BX,SI
	and	BX,07h
	shl	BX,1
	mov	BH,byte ptr EDGES[2+BX]	; BH = last byte

	mov	DX,graph_VramWidth

	sar	SI,3		; SI = num bytes
	js	short RIGHT_EDGE

	; ���݂̃f�[�^ (AH,CX�̓t���[)
	; AL	first byte
	; BH	last byte
	; DX	graph_VramWidth
	; SI	num words
	; DI	start address
	; ES	gseg...
	; CX	abs(y2-y1)+1

	; �Ō�A���h�肾���������� ===========

	mov	BP,DI
	push	CX
	EVEN
LEFT_EDGE:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	DI,DX
	loop	short LEFT_EDGE
	pop	CX

	inc	BP
	mov	DI,BP

	test	SI,SI
	jz	short RIGHT_EDGE1
	EVEN
	mov	AX,BX
	push	CX
BODY:
	mov	BX,SI
REPSTOSB:
	or	byte ptr ES:[DI+BX-1],0ffh
	dec	BX
	jnz	short REPSTOSB
	add	DI,DX
	loop	short BODY
	pop	CX
	mov	BX,AX

	lea	DI,[BP+SI]
RIGHT_EDGE1:
	mov	AL,0ffh

RIGHT_EDGE:
	and	AL,BH
	EVEN
RIGHTLOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	DI,DX
	loop	short RIGHTLOOP
	MRETURN
endfunc			; }

END
