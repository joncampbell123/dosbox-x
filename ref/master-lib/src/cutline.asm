PAGE 98,120
; graphics - cutline
;
; DESCRIPTION:
;	�����̃N���b�s���O
;
; Function/Procedure:
;	curline : near

; Parameters:
;	BL : o1
;	CX : x1
;	DI : y1 - ClipYT
;	BH : o2
;	SI : x2
;	BP : y2 - ClipYT
; Returns:
;	BL,CX,DI ( o1,x1,y1 )�̓N���b�s���O�ɂ��ω�����ꍇ������
;	BH,SI,BP ( o2,x2,y2 )�͕ω����Ȃ��B
;	AX,DX �͔j��
;
; Binding Target:
;	MASM 5.0 / OPTASM / TASM
;
; Running Target:
;	80186/V30/80286...
;
; REQUIRING RESOURCES:
;	CPU: V30
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;
; AUTHOR:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; HISTORY:
;	92/7/27 Initial: gc_line.asm���番��
	.186
	.MODEL SMALL

	.DATA
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYH:WORD

	.CODE
clipasm equ 1
	include clip.inc

; �J�b�g�J�b�g�`�`�`(x1,y1)��ؒf�ɂ��ύX����
; in: BH:o2, BL:o1
; out: BH:o2, BL:o1, o1��zero�Ȃ�� zflag=1
	public cutline
cutline	proc near
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !o2:o1! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+

	or	BL,BL
	jz	short CUTRET
	test	BL,YOKO_OUT
	jz	short CUTTATE
	test	BL,LEFT_OUT
	jz	short CUTAR
		mov	AX,ClipXL
		jmp	short CUTAE
	EVEN
CUTAR:
		mov	AX,ClipXR
CUTAE:
	sub	CX,SI	; CX=x1-x2
	jz	short CUTRET
	mov	DX,DI	; DX=y1-y2
	sub	DX,BP	;
	mov	DI,AX	; DI=x1'
	sub	AX,SI	; AX = (x1'-x2)
	imul	DX	;	* dy
	idiv	CX	;	/ dx
	add	AX,BP	;	+ y2
	mov	CX,DI	; CX=x1'
	mov	DI,AX	; DI=y1'

	xor	AX,AX

	or	DI,DI
	js	short CUTBE
	mov	AX,ClipYH
	cmp	DI,AX
	jg	short CUTBE
CUTIN:	xor	BL,BL	; ���̓_�͒��ɓ�������
CUTRET:	or	BX,BX	; �������S�ɒ��ɂ͂��������ǂ�����z flag�ŕԂ�
	ret
	EVEN

CUTTATE:
	xor	AX,AX
	test	BL,BOTTOM_OUT
	jz	short CUTBE
	mov	AX,ClipYH
CUTBE:
	sub	DI,BP	; DI=y1-y2
	jz	short CUTRET
	mov	DX,CX	; DX=x1-x2
	sub	DX,SI	;
	mov	CX,AX	; CX=y1'
	sub	AX,BP	; AX = (y1'-y2)
	imul	DX	;	* dx
	idiv	DI	;	/ dy
	add	AX,SI	;	+ x2
	mov	DI,CX	; DI=y1'
	mov	CX,AX	; CX=x1'

	cmp	ClipXL,CX
	jle	short CUTBE1
	mov	BL,LEFT_OUT	; ��ł͂����Ă邩��, zflag�� 0 ����
	ret			; �����ɂ����^�[����!!
	EVEN
CUTBE1:	cmp	CX,ClipXR
	jle	short CUTIN
	mov	BL,RIGHT_OUT	; ������ zflag=0
	ret			; �����ɂ����^�[����!!
	EVEN
cutline	endp

END
