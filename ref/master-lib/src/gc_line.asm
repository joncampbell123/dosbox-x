PAGE 98,120
; graphics - grcg - hline - PC98V
;
; DESCRIPTION:
;	�����̕`��(�v���[���̂�)
;
; FUNCTION:
;	void far _pascal grcg_line( int x1, int y1, int y1, int y2 ) ;

; PARAMETERS:
;	int x1,y1	�n�_�̍��W
;	int x1,y2	�I�_�̍��W
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
;	clipline
;
; HISTORY:
;	92/6/7	Initial
;	92/6/8	������1dot�̂Ƃ���/0���N�����Ă����̂����
;		�ϐ���S�ă��W�X�^�Ɋ��蓖��
;		��������g�ݍ���
;	92/6/9	�N���b�s���O�ɕs���B�����̓_���O�ɂ���A�Е���
;		�ؒf��������ƁA������ 1dot�ɂȂ��Ă��܂��ꍇ�A
;		�c��̓_��ؒf���悤�Ƃ���Ƃ��� /0������!������
;	92/6/9	�ȂȂ�45�x��������`�`�`�`������������
;	92/6/10	�R�[�h�T�C�Y�̍œK��
;	92/6/13 ���X����
;	92/6/14
;	92/6/16 TASM�Ή�
;	92/6/22 45�x�������c
;	92/7/27 cutline��cutline.asm�ɐ؂�o��

	.186
	.MODEL SMALL

	.DATA

	; �N���b�s���O�֌W
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	; �������p�̃G�b�W�f�[�^
	EXTRN	EDGES:WORD

	.CODE
	include func.inc
	include clip.inc

ifndef X1280
XBYTES	= 80
YSHIFT4	= 4
else
XBYTES	= 160
YSHIFT4	= 5
endif


; �J�b�g�J�b�g�`�`�`(x1,y1)��ؒf�ɂ��ύX����
; in: BH:o2, BL:o1
; out: BH:o2, BL:o1, o1��zero�Ȃ�� zflag=1

;
MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

;-------------------------------------------------------------
; void far _pascal grcg_line( int x1, int y1, int x2, int y2 ) ;
;
func GRCG_LINE
	push	BP
	push	SI
	push	DI

	CLI
	; parameters
	add	SP,(RETSIZE+3)*2
	pop	BP	; y2
	pop	SI	; x2
	pop	DI	; y1
	pop	CX	; x1
	sub	SP,(RETSIZE+3+4)*2
	STI

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
	jnz	short RETURN1	; �܂����������B�N���b�v�A�E�g�`
	or	BX,BX
	jz	short DRAW_START

	call	cutline		; ��P�_�̃N���b�v
	jz	short DRAW_START
	xchg	BH,BL
	xchg	CX,SI
	xchg	DI,BP
	call	cutline		; ��Q�_�̃N���b�v
	jnz	short RETURN1

	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+

	; �`��J�n =================================
DRAW_START:
	; �Z�O�����g�ݒ�
	mov	ES,ClipYT_seg

	sub	SI,CX			; SI = abs(x2-x1) : deltax
	jnb	short S400
		add	CX,SI		; CX = leftx
		neg	SI
		xchg	DI,BP
S400:
	sub	BP,DI			;
	sbb	DX,DX			;
	mov	BX,XBYTES		; BX(downf) = XBYTES ? -XBYTES
	add	BX,DX			; DX = abs(y2-y1) : deltay
	xor	BX,DX			;
	add	BP,DX			;
	xor	DX,BP			;

	mov	AX,DI			; DI *= 80
	shl	AX,2
	add	DI,AX
	shl	DI,YSHIFT4

	mov	AX,CX
	shr	AX,3
	add	DI,AX			; DI = start adr!
	xor	AX,AX
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+

	; �c�����������̔���
	cmp	SI,DX
	jg	short YOKO_DRAW
	je	short NANAME_DRAW

; �c���̕`�� =================================
TATE_DRAW:
	dec	BX		; BX = downf - 1

	xchg	DX,SI		; DX = deltax, SI = deltay
	div	SI
	mov	DX,8000h
	mov	BP,AX

	and	CL,07h
	mov	AL,DH		; 80h
	shr	AL,CL

	lea	CX,[SI+1]	; CX = deltay + 1

	; �c���̃��[�v ~~~~~~~~~~~~~~~~
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !  bit!8000h!downf! len !deltx!gadr !dx/dy!
	; +-----+-----+-----+-----+-----+-----+-----+
	EVEN
TATELOOP:
	stosb
	add	DX,BP
	jc	short TMIGI
	add	DI,BX
	loop	short TATELOOP
RETURN1:
	MRETURN
TMIGI:	ror	AL,1
	adc	DI,BX
	loop	short TATELOOP
	MRETURN

; �i�i��45�x�̕`�� =================================
NANAME_DRAW:
	dec	BX		; BX = downf - 1
	and	CL,07h
	mov	AL,80h
	shr	AL,CL
	lea	CX,[SI+1]	; CX = deltay + 1

	; 4�����[�v�W�J�c gr.lib 0.9��line.inc�łȂ���Ă������ς�
	; ��������ǂ��|��(^^; �S����������C�ɓ���Ȃ�����ς������ǁc
	shr	CX,1
	jnb	short NANAME1
	stosb
	ror	AL,1
	adc	DI,BX
NANAME1:
	jcxz	short NANAMERET
	shr	CX,1
	jnb	short NANAME2
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
NANAME2:
	jcxz	short NANAMERET

	; �ȂȂ�45�x�̃��[�v ~~~~~~~~~~~~~~~~
	EVEN
NANAMELOOP:
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
	stosb
	ror	AL,1
	adc	DI,BX
	loop	short NANAMELOOP
NANAMERET:
	MRETURN

; �����̕`�� =================================
YOKO_DRAW:
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+
	and	DI,0FFFEh

	or	DX,DX
	jz	short HLINE_DRAW

	div	SI
	mov	DX,8000h
	mov	BP,DX		; BP = 8000h(dd)
	and	CL,0Fh
	shr	DX,CL		; DX = bit
	mov	CX,SI		;
	inc	CX		; CX = deltax + 1
	mov	SI,AX		; SI = deltay / deltax
	xor	AX,AX		; AX = 0(wdata)

	; �����̃��[�v ~~~~~~~~~~~~~~~~
	EVEN
YOKOLOOP:
	or	AX,DX
	ror	DX,1
	jc	short S200
	add	BP,SI
	jc	short S300
	loop	YOKOLOOP
	xchg	AH,AL
	stosw
	MRETURN
S200:	xchg	AH,AL
	stosw
	xor	AX,AX
	add	BP,SI
	jc	short S350
	loop	YOKOLOOP
	MRETURN
S300:	xchg	AH,AL
	mov	ES:[DI],AX
	xor	AX,AX
S350:	add	DI,BX
	loop	YOKOLOOP
	MRETURN

; �������̕`�� ==============================
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!gadr !     !
	; +-----+-----+-----+-----+-----+-----+-----+
HLINE_DRAW:
	mov	BX,CX		; BX = leftx & 15
	and	BX,0Fh		;
	lea	CX,[BX+SI-10h]	; CX = deltax + BX - 16
	shl	BX,1
	mov	AX,[EDGES+BX]	; ���G�b�W
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short HL_LAST
	stosw
	mov	AX,0FFFFh
	rep stosw
HL_LAST:
	and	AX,[EDGES+2+BX]	; �E�G�b�W
	stosw
	MRETURN
endfunc
END
