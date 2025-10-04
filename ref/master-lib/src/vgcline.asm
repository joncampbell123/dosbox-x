; master library - VGA - 16color
;
; Description:
;	�����̕`��
;
; Functions/Procedures:
;	void vgc_line( int x1, int y1, int y1, int y2 ) ;

; Parameters:
;	int x1,y1	�n�_�̍��W
;	int x1,y2	�I�_�̍��W
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
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;	vgc_setcolor()
;	cutline
;
; Revision History:
;	92/6/7	Initial: gc_line.asm
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
;	94/3/9	Arranged by Ara [svgcline.asm]
;	94/4/8 Initial: vgcline.asm/master.lib 0.23 by ����
;		��640dot�ȊO�ɑΉ�

	.186
	.MODEL SMALL
	include func.inc
	include clip.inc
	include vgc.inc

	.DATA

	; �N���b�s���O�֌W
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD

	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	; �������p�̃G�b�W�f�[�^
	EXTRN	EDGES:WORD

	.CODE

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

func VGC_LINE	; vgc_line() {
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
	mov	ES,graph_VramSeg	; �Z�O�����g�ݒ�
	mov	BX,graph_VramWidth

	sub	SI,CX			; SI = abs(x2-x1) : deltax
	jnb	short S400
		add	CX,SI		; CX = leftx
		neg	SI
		xchg	DI,BP
S400:
	mov	AX,DI			; AX = DI * 80
	add	AX,ClipYT
	imul	BX

	sub	BP,DI			;
	sbb	DX,DX			;
	add	BX,DX			; BX(downf) = XBYTES ? -XBYTES
	xor	BX,DX			; DX = abs(y2-y1) : deltay
	add	BP,DX			;
	xor	DX,BP			;

	mov	DI,CX
	shr	DI,3
	add	DI,AX			; DI = start adr!
	xor	AX,AX
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!sad b!     !
	; +-----+-----+-----+-----+-----+-----+-----+

	; �c�����������̔���
	cmp	SI,DX
	jg	short YOKO_DRAW1
	je	short NANAME_DRAW
	jmp	short TATE_DRAW

YOKO_DRAW1:
	jmp	YOKO_DRAW

; �c���̕`�� =================================
TATE_DRAW:
	xchg	DX,SI		; DX = deltax, SI = deltay
	div	SI
;	mov	DX,8000h
	mov	BP,AX

	and	CL,07h
	mov	AL,80h		; 80h
	shr	AL,CL

	lea	CX,[SI+1]	; CX = deltay + 1

	; �c���̃��[�v ~~~~~~~~~~~~~~~~
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !  bit!8000h!downf! len !deltx!gadr !dx/dy!
	; +-----+-----+-----+-----+-----+-----+-----+
	EVEN
	mov	SI,8000h
TATELOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	add	SI,BP
	jc	short TMIGI
	add	DI,BX
	loop	short TATELOOP
RETURN1:
	MRETURN
TMIGI:	ror	AL,1
	adc	DI,BX
	loop	short TATELOOP
	jmp	short RETURN1

; �i�i��45�x�̕`�� =================================
NANAME_DRAW:
	and	CL,07h
	mov	AL,80h
	shr	AL,CL
	lea	CX,[SI+1]	; CX = deltay + 1

	; 4�����[�v�W�J�c gr.lib 0.9��line.inc�łȂ���Ă������ς�
	; ��������ǂ��|��(^^; �S����������C�ɓ���Ȃ�����ς������ǁc
	shr	CX,1
	jnb	short NANAME1
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
NANAME1:
	jcxz	short NANAMERET
	shr	CX,1
	jnb	short NANAME2
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
	test	ES:[DI],AL
	mov	ES:[DI],AL

	ror	AL,1
	adc	DI,BX
NANAME2:
	jcxz	short NANAMERET

	; �ȂȂ�45�x�̃��[�v ~~~~~~~~~~~~~~~~
	EVEN
NANAMELOOP:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	ror	AL,1
	adc	DI,BX

	test	ES:[DI],AL
	mov	ES:[DI],AL
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
	or	DX,DX
	jz	short HLINE_DRAW

	div	SI
	mov	DX,8000h
	mov	BP,DX		; BP = 8000h(dd)
	and	CL,7
	shr	DX,CL		; DX = bit
	mov	CX,SI		;
	inc	CX		; CX = deltax + 1
	mov	SI,AX		; SI = deltay / deltax
	mov	AL,0 		; wdata = 0

	; �����̃��[�v ~~~~~~~~~~~~~~~~
	EVEN
YOKOLOOP:
	or	AL,DH
	ror	DH,1
	jc	short S200
	add	BP,SI
	jc	short S300
	loop	short YOKOLOOP
	test	ES:[DI],AL
	mov	ES:[DI],AL
YOKORET:
	MRETURN
S200:
	test	ES:[DI],AL
	mov	ES:[DI],AL
	inc	DI
	mov	AL,0
	add	BP,SI
	jc	short S350
	loop	YOKOLOOP
	jmp	short YOKORET
S300:
	test	ES:[DI],AL
	mov	ES:[DI],AL

	mov	AL,0
S350:	add	DI,BX
	loop	short YOKOLOOP
	jmp	short YOKORET
	EVEN

; �������̕`�� ==============================
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; ! 0   !delty!downf!leftx!deltx!gadr !     !
	; +-----+-----+-----+-----+-----+-----+-----+
HLINE_DRAW:
	mov	BX,CX		; BX = leftx & 7
	and	BX,7		;
	lea	CX,[BX+SI-8]	; CX = deltax + BX - 8
	shl	BX,1
	mov	AL,byte ptr [EDGES+BX]	; ���G�b�W
	not	AL

	mov	BX,CX		;
	and	BX,7
	shl	BX,1

	sar	CX,3
	js	short HL_LAST
	test	ES:[DI],AL
	mov	ES:[DI],AL
	inc	DI

	mov	AL,0FFh
	jcxz	short HL_LAST
HLINE_REP:
	or	ES:[DI],AL
	inc	DI
	loop	short HLINE_REP
HL_LAST:
	and	AL,byte ptr [EDGES+2+BX]	; �E�G�b�W
	test	ES:[DI],AL
	mov	ES:[DI],AL
	MRETURN
endfunc		; }
END
