; master library - gc_poly - grcg - circle
;
; Description:
;	�~�`��
;
; Function/Procedures:
;	void grcg_circle( int x, int y, unsigned r ) ;
;
; Parameters:
;	x,y	���S���W
;	r	���a ( 1 �ȏ� )
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	���a�� 0 �Ȃ�Ε`�悵�܂���B
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 5/12 Initial:gc_circl.asm/master.lib 0.16
;	93/ 5/14 [M0.16] ����
;	95/ 2/20 �c�{���[�h�Ή�
;	95/ 3/25 [M0.22k] �S�~���ł�o�O�C��

	.186
	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD
	EXTRN	graph_VramZoom:WORD


	.DATA?
cx_	DW	?
cy_	DW	?

	.CODE
	EXTRN GRCG_CIRCLE_X:CALLMODEL

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	6
	EVEN
	endm

retfunc ALLIN
	pop	SI
	pop	BP
	jmp	GRCG_CIRCLE_X
CLIPOUT:
	pop	SI
	pop	BP
	ret	6
	EVEN
endfunc

func GRCG_CIRCLE	; grcg_circle() {
	push	BP
	mov	BP,SP
	push	SI
	; DI�͉��̕���push���Ă�

	; ����
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	r	= (RETSIZE+1)*2

	mov	SI,[BP+r]	; SI = ���a
	xor	CX,CX		; �ǂ����N���b�v�g�ɂЂ����������� 0�ȊO�Ɂ[

	; ��܂��ȃN���b�s���O
	mov	AX,[BP+y]
	sub	AX,ClipYT
	mov	DX,AX		; DX = y
	sub	AX,SI
	cmp	AX,ClipYH
	jg	short CLIPOUT
	shl	AX,1
	rcl	CX,1
	mov	AX,DX
	add	AX,SI
	js	short CLIPOUT
	cmp	ClipYH,AX
	rcl	CX,1

	mov	AX,[BP+x]
	mov	BX,AX		; BX = x
	sub	AX,SI
	cmp	AX,ClipXR
	jg	short CLIPOUT
	sub	AX,ClipXL
	shl	AX,1
	rcl	CX,1
	mov	AX,BX
	add	AX,SI
	cmp	AX,ClipXL
	jl	short CLIPOUT
	cmp	ClipXR,AX
	rcl	CX,1
	jcxz	short ALLIN

	; ���a�� 0 �Ȃ�`�悵�Ȃ�
	or	SI,SI
	jz	short CLIPOUT

	push	DI

	mov	AL,byte ptr graph_VramZoom
	mov	CS:_VRAMZOOM1,AL
	mov	CS:_VRAMZOOM2,AL

	mov	ES,ClipYT_seg
	mov	AX,BX		; AX = x

	mov	cx_,AX
	mov	cy_,DX
	xor	DI,DI
	mov	BP,SI
	EVEN
LOOPTOP:
	call	PSET4
	cmp	SI,DI
	je	short SKIP
	xchg	SI,DI
	call	PSET4
	xchg	SI,DI
SKIP:
	stc		; BP -= DI*2+1;
	sbb	BP,DI	;
	sub	BP,DI	;
	jns	short LOOPCHECK
	dec	SI	; --SI;
	add	BP,SI	; BP += SI*2;
	add	BP,SI	;
LOOPCHECK:
	inc	DI	; ++DI;
	cmp	SI,DI
	jge	short LOOPTOP
	MRETURN
endfunc			; }

; in:
;	SI = dx
;	DI = dy
; break:
;	AX,BX,CX,DX
;
	EVEN
PSET4Q proc near
	retn
	EVEN
PSET4Q endp

PSET4 proc	near
	mov	AX,cy_
	mov	BX,DI
	shr	BX,9	; dummy
	org	$-1
_VRAMZOOM1	db	?
	or	BX,BX
	jz	short PS4
	sub	AX,BX
	js	short PS3
	cmp	AX,ClipYH
	jg	short PSET4Q	; ���`����킯���Ȃ��ꍇ

	; AX = (cy_ - DI)
	; (cx_ + SI, AX)�� (cx_ - SI, AX)�ɓ_��ł�
	mov	DX,AX
	shl	DX,2
	add	DX,AX
	shl	DX,4

	mov	AX,cx_

	or	SI,SI		; dx�� 0 �Ȃ�� 1 �_�̂�
	je	short PS2

	sub	AX,SI
	cmp	AX,ClipXL
	jl	short PS1
	cmp	AX,ClipXR
	jg	short PSET4Q	; �����̓_���E�[���z���Ă���Ȃ�
				; �E���̓_�͕`����킯���Ȃ�

	mov	BX,AX
	mov	CX,AX
	shr	BX,3
	add	BX,DX
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	ES:[BX],CH
PS1:
	mov	AX,cx_
	add	AX,SI

PS2:
	cmp	AX,ClipXL
	jl	short PS3
	cmp	AX,ClipXR
	jg	short PS3

	mov	BX,AX
	mov	CX,BX
	shr	BX,3
	add	BX,DX
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	ES:[BX],CH

PS3:
	mov	AX,cy_
	mov	BX,DI
	shr	BX,9	; dummy
	org	$-1
_VRAMZOOM2	db	?
	add	AX,BX
PS4:
	cmp	AX,ClipYH
	jae	short PSQ

	; (cx_ + SI, AX)�� (cx_ - SI, AX)�ɓ_��ł�
	mov	DX,AX
	shl	DX,2
	add	DX,AX
	shl	DX,4

	mov	AX,cx_

	or	SI,SI		; dx�� 0 �Ȃ�� 1 �_�̂�
	je	short PS6

	sub	AX,SI
	cmp	AX,ClipXL
	jl	short PS5
	cmp	AX,ClipXR
	jg	short PSQ	; �����̓_���E�[���z���Ă���Ȃ�
				; �E���̓_�͕`����킯���Ȃ�

	mov	BX,AX
	mov	CX,AX
	shr	BX,3
	add	BX,DX
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	ES:[BX],CH
PS5:
	mov	AX,cx_
	add	AX,SI

PS6:
	cmp	AX,ClipXL
	jl	short PSQ
	cmp	AX,ClipXR
	jg	short PSQ

	mov	BX,AX
	mov	CX,BX
	shr	BX,3
	add	BX,DX
	and	CL,7
	mov	CH,80h
	shr	CH,CL
	mov	ES:[BX],CH

PSQ:
	ret
PSET4 endp

END
