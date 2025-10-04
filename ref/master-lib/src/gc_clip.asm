; graphics - clipping
;
; DESCRIPTION:
;	�`��N���b�v�g�̐ݒ菈��
;
; FUNCTION(in C):
;	int far _pascal grc_setclip( int xl, int yt, int xr, int yb ) ;
;
; FUNCTION(in Pascal):
;	function grc_setclip( xl,yt,xr,yb : integer ) : integer ;
;
; PARAMETERS:
;	int xl	���[��x���W
;	int yt	��[��y���W
;	int xr	�E�[��x���W
;	int yb	���[��y���W
;
; RETURNS:
;	0 = ���s�B�����(640x400)�̊O�ɃN���b�v�̈��ݒ肵�悤�Ƃ���
;	1 = �����B
;
; NOTES:
;	�E���͂́A�܂���ʘg(0,0)-(639,399)�ŃN���b�v���Ă���B
;	�E���͂̑΂́A�召�֌W���t�ł������Ă��珈�����Ă���B
;
; GLOBAL VARIABLES:
;	int ClipXL, ClipXW, ClipXR	���[x, �g�̉���(xr-xl), �E�[x
;	int ClipYT, ClipYH, ClipYB	��[y, �g�̍���(yb-yt), ���[y
;	unsigned ClipYB_adr		���[�̍��[��VRAM�I�t�Z�b�g
;	unsigned ClipYT_seg		��[��VRAM�Z�O�����g(�v���[��)
;
; REFER:
;	�E�Ή��֐�
;	  grcg_polygon_convex()
;	  grcg_trapezoid()
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; AUTHOR:
;	���ˏ��F
;
; HISTORY:
;	92/5/20 Initial
;	92/6/5	_pascal���������A�o�O�o���B�Ё[�B�������B�Ԃ�
;	92/6/5	ClipYB_adr�ǉ��B
;	92/6/6	ClipYT_seg, GRamStart�ǉ��B
;	92/6/16 �f�[�^������clip.asm�ɕ���
;	93/3/27 master.lib�ɍ���
;	93/12/28 [M0.22] ���̐�����graph_VramWidth*8�h�b�g�ɂ���
;

.186
.MODEL SMALL
.DATA
	extrn ClipXL:word, ClipXW:word, ClipXR:word
	extrn ClipYT:word, ClipYH:word, ClipYB:word
	extrn ClipYT_seg:word, ClipYB_adr:word
	extrn graph_VramSeg:word
	extrn graph_VramLines:word
	extrn graph_VramWidth:word	; 16�̔{���ł��邱��!!

.CODE
	include func.inc

;	int far _pascal grc_setclip( int xl, int yt, int xr, int yb ) ;
func GRC_SETCLIP
	push	BP
	mov	BP,SP

	; ����
	xl = (RETSIZE+4)*2
	yt = (RETSIZE+3)*2
	xr = (RETSIZE+2)*2
	yb = (RETSIZE+1)*2

;------------------------- �w���W
	mov	AX,[BP+xl]
	mov	BX,[BP+xr]

	test	AX,BX		; AX < 0 && BX < 0 �Ȃ�G���[
	js	short ERROR

	cmp	AX,BX
	jl	short A
	xchg	AX,BX		; AX <= BX�ɂ���
A:
	cmp	AX,8000h	;
	sbb	DX,DX		;
	and	AX,DX		; AX�������Ȃ�[���ɂ���

	mov	CX,graph_VramWidth
	shl	CX,3
	dec	CX
	sub	BX,CX		;
	sbb	DX,DX		;
	and	BX,DX		;
	add	BX,CX		; BX��639�ȏ�Ȃ�639�ɂ���

	sub	BX,AX
	jl	short ERROR

	mov	ClipXL,AX
	mov	ClipXW,BX
	add	AX,BX
	mov	ClipXR,AX

;------------------------- �x���W

	mov	AX,[BP+yt]
	mov	BX,[BP+yb]

	test	AX,BX		; AX < 0 && BX < 0 �Ȃ�G���[
	js	short ERROR

	cmp	AX,BX
	jl	short B
	xchg	AX,BX		; AX <= BX�ɂ���
B:
	cmp	AX,8000h	;
	sbb	DX,DX		;
	and	AX,DX		; AX�������Ȃ�[���ɂ���

	mov	CX,graph_VramLines
	dec	CX
	sub	BX,CX		;
	sbb	DX,DX		;
	and	BX,DX		;
	add	BX,CX		; BX��399�ȏ�Ȃ�399�ɂ���

	sub	BX,AX
	jl	short ERROR

	mov	ClipYT,AX
	mov	CX,AX
	mov	ClipYH,BX
	add	AX,BX
	mov	ClipYB,AX

	mov	AX,graph_VramWidth
	xchg	AX,BX
	mul	BX
	mov	ClipYB_adr,AX	; ���[y�̍��[��VRAM�I�t�Z�b�g
				; (ClipYT_seg�Z�O�����g��)

	mov	AX,BX
	shr	AX,4
	mul	CX
	add	AX,graph_VramSeg ; �J�n�Z�O�����g = graph_VramSeg
	mov	ClipYT_seg,AX	;                 + yt * graph_VramWidth/16

	mov	AX,1
	pop	BP
	ret	8

ERROR:
	xor	AX,AX
	pop	BP
	ret	8
endfunc
END
