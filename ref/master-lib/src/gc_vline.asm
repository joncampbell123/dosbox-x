PAGE 98,120
; graphics - grcg - vline - PC98V
;
; DESCRIPTION:
;	�������̕`��(�v���[���̂�)
;
; FUNCTION:
;	void grcg_vline( int x, int y1, int y2 ) ;

; PARAMETERS:
;	int x	x���W
;	int y1	���y���W
;	int y2	����y���W
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;	�Ey1,y2�̏㉺�֌W�͋t�ł�(���x��)�S���ς��Ȃ����삵�܂��B
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; HISTORY:
;	92/10/5	Initial
;	93/10/15 [M0.21] �c�N���b�s���O��bugfix (iR����)
;	93/11/26 [M0.22] �������(;_;) y�̏������ق��������Ɓc
;	94/ 3/12 [M0.23] ���ʂȂ��Ƃ���Ă��̂ŏk��

	.186

	.MODEL SMALL


	.DATA?

		EXTRN	ClipXL:WORD, ClipXR:WORD
		EXTRN	ClipYT:WORD, ClipYH:WORD
		EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc

MRETURN macro
	pop	DI
	pop	BP
	ret	6
	EVEN
	endm

func GRCG_VLINE
	push	BP
	mov	BP,SP
	push	DI

	; PARAMETERS
	x  = (RETSIZE+3)*2
	y1 = (RETSIZE+2)*2
	y2 = (RETSIZE+1)*2

	mov	AX,ClipYT
	mov	CX,ClipYH

	mov	BX,[BP+y1]
	mov	DX,[BP+y2]
	cmp	BX,DX
	jl	short GO
	xchg	BX,DX
GO:

	; y�̕ϊ��ƃN���b�v
	sub	DX,AX
	jl	short RETURN
	sub	BX,AX
	cmp	BH,80h
	sbb	DI,DI
	and	BX,DI

	cmp	BX,CX
	jg	short RETURN
	sub	DX,CX		; DX�͕��ɂȂ肦�Ȃ�(Clip�g�����ł���O��)
	sbb	DI,DI
	and	DX,DI
	add	DX,CX

	mov	AX,[BP+x]	; x�̃N���b�v
	cmp	AX,ClipXL
	jl	short RETURN
	cmp	AX,ClipXR
	jg	short RETURN
	mov	CX,AX
	and	CL,07h
	shr	AX,3
	mov	DI,AX
	mov	AL,80h
	shr	AL,CL

	mov	CX,DX		; �����̌v�Z
	mov	DX,80-1		; DX������(stosb�ő�����1�����炩���ߌ��炷)
	sub	CX,BX		; CX�͒���
	imul	BX,BX,80
	add	DI,BX

	mov	ES,ClipYT_seg	; �Z�O�����g�ݒ�
	inc	CX		; �������o��(abs(y2-y1)+1)

	shr	CX,1
	jnb	short S1
	stosb
	add	DI,DX
S1:
	shr	CX,1
	jnb	short S2
	stosb
	add	DI,DX
	stosb
	add	DI,DX
S2:	jcxz	short RETURN
	EVEN
L:	stosb
	add	DI,DX
	stosb
	add	DI,DX
	stosb
	add	DI,DX
	stosb
	add	DI,DX
	loop	short L

RETURN:
	MRETURN
endfunc
END
