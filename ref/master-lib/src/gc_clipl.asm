PAGE 98,120
; graphics - clip - line
;
; DESCRIPTION:
;	�����̕`��(�v���[���̂�)
;
; FUNCTION:
;	int _pascal grc_clip_line( Point * p1, Point * p2 ) ;
;
; PARAMETERS:
;	Point * p1	�n�_�̍��W
;	Point * p2	�I�_�̍��W
;
; Returns:
;	0	���S�ɘg�O(�N���b�s���O�ɂ���ď��ł���)
;	1	�g���̒������W���������ꂽ
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	8086
;
; REQUIRING RESOURCES:
;	CPU: 8086
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�g�O�̎��� *p1, *p2 �̒l�͕ω����܂���B
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;	clipline
;
; HISTORY:
;	93/ 3/ 1 Initial ( from gc_line.asm )
;	93/ 6/ 6 [M0.19] small data model�ł̖\��fix

	.MODEL SMALL

	.DATA

	; �N���b�s���O�֌W
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc
	include clip.inc

; �J�b�g�J�b�g�`�`�`(x1,y1)��ؒf�ɂ��ύX����
; in: BH:o2, BL:o1
; out: BH:o2, BL:o1, o1��zero�Ȃ�� zflag=1

;
MRETURN macro
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	2*DATASIZE*2
	EVEN
	endm

;-------------------------------------------------------------
; int _pascal grc_clip_line( Point *p1, Point *p2 ) ;
;
func GRC_CLIP_LINE
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS
	PUSHED = (DATASIZE+1)*2	; SI,DI,[DS]

	; ����
	p1	= (RETSIZE+1+DATASIZE)*2
	p2	= (RETSIZE+1)*2

	_lds	BX,[BP+p1]
	mov	CX,[BX]		; x1
	mov	DI,[BX+2]	; y1

	_lds	BX,[BP+p2]
	mov	SI,[BX]		; x2
	mov	BP,[BX+2]	; y2

	; �N���b�s���O�J�n =================================

	; �N���b�v�g�ɍ��킹��y���W��ϊ�
	mov	AX,ClipYT
	sub	DI,AX
	sub	BP,AX

	mov	AX,ClipXL
	mov	DX,ClipYH
	mov	BX,0505h	; outcode ��S��������!!
	GETOUTCODE BL,CX,DI,AX,ClipXR,DX
	GETOUTCODE BH,SI,BP,AX,ClipXR,DX
	test	BH,BL
	jnz	short CLIPOUT	; �܂����������B�N���b�v�A�E�g�`
	or	BX,BX
	jz	short OK

	call	cutline		; ��P�_�̃N���b�v
	jz	short OK
	xchg	BH,BL
	xchg	CX,SI
	xchg	DI,BP
	call	cutline		; ��Q�_�̃N���b�v
	jnz	short CLIPOUT

	xchg	CX,SI		; �O��֌W�����Ƃɂ��ǂ�
	xchg	DI,BP

	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+
OK:
	mov	AX,BP
	mov	BP,SP
	_lds	BX,[BP+p1+PUSHED]
	mov	[BX],CX		; x1
	mov	[BX+2],DI	; y1

	_lds	BX,[BP+p2+PUSHED]
	mov	[BX],SI		; x2
	mov	[BX+2],AX	; y2
	mov	AX,1
	MRETURN

	; �N���b�v�A�E�g
CLIPOUT:
	xor	AX,AX
	MRETURN
endfunc

END
