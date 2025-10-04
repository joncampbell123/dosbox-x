; gc_poly library - clip - polygon - convex
;
; Description:
;	��/�����p�`�̃N���b�s���O
;
; Funciton/Procedures:
;	int grc_clip_polygon_n( Point * dest, int ndest,
;				const Point * src, int nsrc ) ;
;
; Parameters:
;	Point *dest	�N���b�v���ʊi�[��
;	int ndest	dest�Ɋm�ۂ��Ă���v�f��
;	Point *src	�N���b�v���̑��p�`�̒��_�z��
;	int nsrc	src�Ɋ܂܂��_�̐�
;
; Returns:
;	-1: src�̂܂܂łn�j�ł��B
;	0: �S���͈͊O�ł��B�`��ł��܂���B
;	1�`: dest�Ƀf�[�^���i�[���܂����B�_����Ԃ��܂��B
;
;	��L�R��ށA�ǂ̏ꍇ�ł� dest�̓��e�͕ω����܂��B
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
; 	�Edest�ɂ́A�ʂ̏ꍇnsrc+4��,���̏ꍇnsrc*2�ȏ���m�ۂ��Ă������ƁB
;	�@���s���ɁA���� ndest �S�Ă����������܂��B
;
;	�E�����p�`�����͂���A�N���b�s���O�̌��ʂ������̓��ɂȂ��Ă��܂��ꍇ�A
;	�@�N���b�v�g�̎��͂ɉ����čׂ��q�����`�ɂȂ�܂��B
;
;	�Edest��src�̊֌W�́A�S�������ꍇ�ƁA�S���d�Ȃ�Ȃ��ꍇ��
;	�@�ǂ��炩�łȂ��Ă͂����܂���B
;	�@dest=src�̏ꍇ�A�߂�l�� -1 ���Ԃ鎖�͂���܂���B
;
;	�Esrc �܂��� dest �� NULL ���ǂ����̌����͍s���Ă��܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/ 7/11 Initial
;	92/ 7/17 bugfix(y=ClipYT or YB�̓_���܂ރN���b�v�̌��)
;	92/ 7/30 bugfix(FAR�ŁAsrc��dest�̈�v����ɃZ�O�����g�������Ă���)
;	92/11/16 bugfix(FAR�ŁAret <popvalue>�̒l���Ԉ���Ă���
;	93/ 5/10 bugfix(FAR�ŁA�ЂƂZ�O�����g���[�h������Ȃ�����(^^;)
;
	.186
	.MODEL SMALL
	.DATA?

	; �N���b�v�g
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD

	.CODE
	include clip.inc
	include func.inc

; IN: x
; OUT: ans
; BREAK: ans, flag
GETOUTCODE_X macro ans,x,xl,xr
	local OUTEND
	mov	ans,LEFT_OUT
	cmp	xl,x
	jg	short OUTEND
	xor	ans,YOKO_OUT
	cmp	x,xr
	jg	short OUTEND
	xor	ans,ans
OUTEND:
	ENDM

; IN: y
; OUT: ans
; BREAK: ans, flag
GETOUTCODE_Y macro ans,y,yt,yb
	local OUTEND
	mov	ans,BOTTOM_OUT
	cmp	y,yb
	jg	short OUTEND
	xor	ans,TATE_OUT
	cmp	y,yt
	jl	short OUTEND
	xor	ans,ans
OUTEND:
	ENDM

MRETURN macro
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	(2+datasize*2)*2
	EVEN
	endm

if datasize EQ 2
LODE	macro	dest,src
	mov	dest,ES:src
	endm
STOE	macro	dest,src
	mov	ES:dest,src
	endm
ESSRC	macro
	mov	ES,[BP+src+2]
	endm
ESDEST	macro
	mov	ES,[BP+dest+2]
	endm
else
LODE	macro	dest,src
	mov	dest,src
	endm
STOE	macro	dest,src
	mov	dest,src
	endm
ESSRC	macro
	endm
ESDEST	macro
	endm
endif

;---------------------------------------
; int grc_clip_polygon_n( Point * dest, int ndest,
;			const Point * src, int nsrc ) ;
func GRC_CLIP_POLYGON_N
	ENTER	14,0
	push	SI
	push	DI

	; ����
	dest	= (RETSIZE+3+DATASIZE)*2
	ndest	= (RETSIZE+2+DATASIZE)*2
	src	= (RETSIZE+2)*2
	nsrc	= (RETSIZE+1)*2

	; ���[�J���ϐ�
	yb = -2			; STEP 1 �̂�

	d = -2			; dx,dy
	oc = -4
	y = -6
	x = -8
	radr = -10
	last_oc = -12
	i = -14

	;---------------------------
	; �ŏ��̃X�e�b�v:
	;		���S����i���S�ɒ����A���S�ɊO���j

	; src
	; nsrc
	; (i,x,y)
	; (xl,xr,yt,yb)

	mov	BX,[BP+nsrc]	;
	dec	BX
	shl	BX,2		; 
	_les	SI,[BP+src]

	; xl,xr,yt,yb�̏����ݒ�
	LODE	AX,[SI+BX]
	mov	DX,AX		; DX = xl
	mov	CX,AX		; CX = xr
	LODE	AX,[SI+BX+2]
	mov	DI,AX		; DI = yt
	mov	[BP+yb],AX
	sub	BX,4
	; ���[�v�`
LOOP1:
	LODE	AX,[SI+BX]
	cmp	AX,DX
	jg	short L1_010
	mov	DX,AX
L1_010:	cmp	AX,CX
	jl	short L1_020
	mov	CX,AX
L1_020:
	LODE	AX,[SI+BX+2]
	cmp	AX,DI
	jg	short L1_030
	mov	DI,AX
L1_030:	cmp	AX,[BP+yb]
	jle	short L1_040
	mov	[BP+yb],AX
L1_040:	sub	BX,4
	jge	short LOOP1

	; �S�_�������I�����̂ŁABBOX�ɂ����O����
LOOP1E:
	; �O����
	cmp	ClipXL,CX	; CX=xr
	jg	short RET_OUT
	cmp	DX,ClipXR	; DX=xl
	jg	short RET_OUT
	cmp	ClipYB,DI	; DI=yt
	jl	short RET_OUT
	mov	AX,[BP+yb]
	cmp	ClipYT,AX
	jg	short RET_OUT

	; ������
	cmp	DX,ClipXL	; DX=xl
	jl	short STEP2
	cmp	CX,ClipXR	; CX=xr
	jg	short STEP2
	cmp	DI,ClipYT
	jl	short STEP2
	mov	AX,ClipYB
	cmp	[BP+yb],AX
	jg	short STEP2
	mov	AX,[BP+dest]
	cmp	SI,AX		; SI = src
	jne	short RET_IN
if datasize EQ 2
	mov	AX,ES		; segment compare
	mov	SI,[BP+dest+2]
	cmp	SI,AX
	jne	short RET_IN
endif
	mov	AX,[BP+nsrc]
	MRETURN

RET_IN:	; ���S�ɒ�����
	mov	AX,-1
	MRETURN

RET_OUT: ; ���S�ɊO����
	xor	AX,AX
	MRETURN

	;---------------------------
	; ��Q�X�e�b�v:
	;		�㉺�̃J�b�g
STEP2:
	mov	SI,[BP+ndest]	; SI=nans

	_les	BX,[BP+src]
	LODE	AX,[BX]
	mov	[BP+x],AX	; x = src[0].x
	LODE	AX,[BX+2]
	mov	[BP+y],AX	; y = src[0].y
	GETOUTCODE_Y	BX,AX,ClipYT,ClipYB
	mov	[BP+last_oc],BX	; last_oc = outcode_y(x,y)
	mov	DI,[BP+nsrc]
	dec	DI
	jns	short STEP2_1
	jmp	STEP3		; ������ʂ�Ƃ���SI=nans�łȂ��Ⴂ�����
				; �ύX����Ƃ��͋C��t���悤

STEP2_1:
	mov	AX,DI
	shl	AX,2
	add	AX,[BP+src]
	mov	[BP+radr],AX	; radr = &src[nsrc-1]
	mov	[BP+i],DI	; i = nsrc-1
	mov	DI,[BP+y]	; DI=y
LOOP2:
	mov	DX,[BP+x]	; DX=x
	mov	CX,DI		; CX=y
	mov	BX,[BP+radr]
	ESSRC
	LODE	AX,[BX]		; AX = x = radr->x
	mov	[BP+x],AX	;
	LODE	DI,[BX+2]	; DI = radr->y
	cmp	DX,AX
	jne	short S2_010
	cmp	CX,DI
	jne	short S2_010
	jmp	S2_090		; (;_;)
S2_010:
	sub	DX,AX
	sub	CX,DI
	GETOUTCODE_Y	AX,DI,ClipYT,ClipYB
	mov	[BP+oc],AX
	mov	BX,[BP+last_oc]
	cmp	AX,BX
	je	short S2_070

	mov	[BP+d],DX	; dx

	; �����̎n�_���̐ؒf
	or	BX,BX
	je	short S2_040

	cmp	BX,TOP_OUT
	jne	short S2_020
	mov	AX,ClipYT
	jmp	short S2_030
S2_020:	mov	AX,ClipYB
S2_030:	cmp	AX,DI		;y
	je	short S2_040

	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	STOE	[BX+2],AX	; wy
	sub	AX,DI		; y
	imul	DX		; dx
	idiv	CX		; dy
	add	AX,[BP+x]
	STOE	[BX],AX

S2_040:
	; �����̏I�_���̐ؒf
	mov	BX,[BP+oc]
	or	BX,BX
	je	short S2_070
	cmp	BX,TOP_OUT
	jne	short S2_050
	mov	AX,ClipYT
	jmp	short S2_060
	EVEN
S2_050:	mov	AX,ClipYB
S2_060:
	mov	DX,CX		; dy
	add	DX,DI		;y
	cmp	DX,AX
	je	short S2_070

	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	STOE	[BX+2],AX

	sub	AX,DI		;y
	imul	WORD PTR [BP+d]	; dx
	idiv	CX		; dy
	add	AX,[BP+x]
	STOE	[BX],AX
S2_070:
	mov	CX,[BP+oc]
	or	CX,CX
	jne	short S2_080
	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	mov	AX,[BP+x]
	STOE	[BX],AX
	STOE	[BX+2],DI	;y
S2_080:
	mov	[BP+last_oc],CX
S2_090:
	sub	WORD PTR [BP+radr],4
	dec	WORD PTR [BP+i]
	js	short STEP3
	jmp	LOOP2		; (;_;)

	;---------------------------
	; ��R�X�e�b�v:
	;		���E�̃J�b�g
	; SI: nans
STEP3:
	mov	CX,[BP+ndest]
	mov	BX,CX
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	LODE	AX,[BX-2]
	mov	[BP+y],AX
	LODE	DI,[BX-4]
	GETOUTCODE_X	AX,DI,ClipXL,ClipXR
	mov	[BP+last_oc],AX
	cmp	SI,CX
	jl	short S3_010
	xor	AX,AX	; nans=0������
	jmp	RETURN
	EVEN
S3_010:
	mov	AX,SI
	shl	AX,2
	add	AX,[BP+dest]
	mov	[BP+radr],AX
	mov	AX,CX
	sub	AX,SI
	mov	[BP+i],AX
	mov	SI,0		; nans=0
LOOP3:
	mov	CX,DI		; CX:dx
	mov	AX,[BP+y]
	mov	DX,AX		; DX:dy
	mov	BX,[BP+radr]
	LODE	DI,[BX]		; DI:x
	LODE	AX,[BX+2]
	mov	[BP+y],AX
	sub	CX,DI
	sub	DX,AX

	mov	BX,DI
	GETOUTCODE_X	AX,BX,ClipXL,ClipXR
	mov	[BP+oc],AX
	mov	BX,[BP+last_oc]
	cmp	AX,BX
	je	short S3_070

	mov	[BP+d],DX	; dy

	or	BX,BX
	je	short S3_040
	cmp	BX,LEFT_OUT
	jne	short S3_020
	mov	AX,ClipXL
	jmp	short S3_030
	EVEN
S3_020:
	mov	AX,ClipXR
S3_030:
	cmp	AX,DI
	je	short S3_040
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],AX
	sub	AX,DI		; x
	imul	DX
	idiv	CX
	add	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_040:
	mov	BX,[BP+oc]
	or	BX,BX
	je	short S3_070
	cmp	BX,LEFT_OUT
	jne	short S3_050
	mov	AX,ClipXL
	jmp	SHORT S3_060
S3_050:
	mov	AX,ClipXR
S3_060:
	mov	BX,CX
	add	BX,DI
	cmp	BX,AX
	je	short S3_070
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],AX
	sub	AX,DI		; x
	imul	WORD PTR [BP+d]	; dy
	idiv	CX	; dx
	add	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_070:
	mov	CX,[BP+oc]
	or	CX,CX
	jne	short S3_080
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],DI
	mov	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_080:
	mov	[BP+last_oc],CX
	add	WORD PTR [BP+radr],4
	dec	WORD PTR [BP+i]
	je	short S3_090
	jmp	LOOP3		; (;_;)
S3_090:
	mov	AX,SI
RETURN:
	MRETURN
endfunc

END
