PAGE 98,120
; graphics - grcg - pset - PC98V
;
; DESCRIPTION:
;	�_�̕`��
;
; FUNCTION:
;	void far _pascal grcg_pset( int x, int y )

; PARAMETERS:
;	int x,y		�_�̍��W
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
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; HISTORY:
;	92/6/12	Initial
;	92/7/16 bugfix(^^;

	.186
	.MODEL SMALL

	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc

func GRCG_PSET
	mov	BX,BP	; save BP
	mov	BP,SP

	; parameters
	x = (RETSIZE+1)*2
	y = (RETSIZE+0)*2

	mov	CX,[BP+x]
	mov	DX,[BP+y]

	mov	BP,BX	; restore BP

	cmp	CX,ClipXL
	jl	short RETURN
	cmp	CX,ClipXR
	jg	short RETURN
	sub	DX,ClipYT
	jl	short RETURN
	cmp	DX,ClipYH
	jg	short RETURN

	mov	AX,DX
	shl	AX,2
	add	DX,AX
	add	DX,ClipYT_seg
	mov	ES,DX

	mov	BX,CX
	shr	BX,3
	and	CL,7
	mov	AL,80h
	shr	AL,CL

	mov	ES:[BX],AL
RETURN:
	ret 4
endfunc
END
