; superimpose & master library module
;
; Description:
;	�p�^�[���̕\��(16x16����, 4�F�ȓ�, ��ʏ㉺�A��)
;
; Functions/Procedures:
;	void super_roll_put_tiny( int x, int y, int num ) ;
;
; Parameters:
;	x,y	�`�悷����W
;	num	�p�^�[���ԍ�
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	���炩���߁Asuper_convert_tiny �Ńf�[�^��ϊ����Ă����K�v�A��
;	�f�[�^�`��: (super_patdata[])
;		--------------------------
;		1���[�h = (�ЂƂ߂̐F�R�[�h << 8) + 80h
;		32�o�C�g = �f�[�^
;		--------------------------
;		1���[�h = (�ӂ��߂̐F�R�[�h << 8) + 80h
;		32�o�C�g = �f�[�^
;		�c
;		--------------------------
;		1���[�h = 0 (�I���)
;
;	�F�� 4�F��葽���ƁA�f�[�^�������傫���Ȃ�̂ŕϊ�����Ƃ�����
;
;	�V���[�e�B���O�Q�[���Ȃ񂩂ŁA��ʂɕ\�������u�e�v�́A
;	16x16dot �ȓ��łł�����?����Ȃ炱��ō����ɂł����
;
;	y�̎w��\�͈͂́A0 �` 399
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	93/ 9/19 Initial: superrpt.asm / master.lib 0.21
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patdata:WORD

WIDE_RANGE	equ 0		; 1 �Ȃ� y�� -400�`784�Ή�

PATTERN_HEIGHT	equ 16
SCREEN_HEIGHT	equ 400
SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; ���������ޯĂ������Ă����ޯĂ����ڼ޽����珑��

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	ret	6
	EVEN
	endm

func SUPER_ROLL_PUT_TINY	; super_roll_put_tiny() {
	mov	BX,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+2)*2
	y	= (RETSIZE+1)*2
	num	= (RETSIZE+0)*2

	mov	CX,SS:[BX+x]
	mov	DI,SS:[BX+y]
	mov	BX,SS:[BX+num]
	shl	BX,1
	mov	DS,super_patdata[BX]

IF WIDE_RANGE ne 0
	rol	DI,1
	sbb	DX,DX
	and	DX,SCREEN_HEIGHT	; ���Ȃ��ʏc�Ԃ񉺂ɂ��炷
	ror	DI,1
	add	DI,DX
ENDIF

	; SCREEN_XBYTES = 80 ��z��
	mov	BX,DI
	shl	BX,2
	add	BX,DI
	shl	BX,4
	mov	AX,CX
	and	CX,7		; CL = x & 7
	shr	AX,3
	add	BX,AX		; BX = draw start offset

	xor	SI,SI
	mov	AX,0a800h
	mov	ES,AX

	lodsw
	cmp	AL,80h
	jnz	short RETURN	; �P�F���Ȃ��Ƃ�(��������)

	mov	AL,GC_RMW
	pushf
	CLI
	out	7ch,AL
	popf

	mov	DL,0ffh		; DL = ���[�h���E�}�X�N
	shr	DL,CL

	test	BX,1
	jnz	short ODD_COLOR_LOOP
	EVEN
	; �����A�h���X
EVEN_COLOR_LOOP:
	; �F�̎w�肠���
	REPT 4
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,PATTERN_HEIGHT
	mov	DI,BX
	cmp	DI,(SCREEN_HEIGHT-PATTERN_HEIGHT+1)*SCREEN_XBYTES
	jb	short EVEN_YLOOP2
IF WIDE_RANGE ne 0
	jmp	short EVEN_YLOOP1_ENTRY
ENDIF

	EVEN
EVEN_YLOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES
	dec	CH
IF WIDE_RANGE ne 0
	jz	short EVEN_YLOOP_END
EVEN_YLOOP1_ENTRY:
ENDIF
	cmp	DI,SCREEN_HEIGHT*SCREEN_XBYTES
	jb	short EVEN_YLOOP1

	sub	DI,SCREEN_HEIGHT*SCREEN_XBYTES

	EVEN
EVEN_YLOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short EVEN_YLOOP2

EVEN_YLOOP_END:
	lodsw
	cmp	AL,80h
	je	short EVEN_COLOR_LOOP

	; ����ȃf�[�^�Ȃ�� AL=0������c(�Ђ�)
	out	GC_MODEREG,AL		; grcg off

RETURN:
	MRETURN


	EVEN
	; ��A�h���X
ODD_COLOR_LOOP:
	; �F�̎w�肠���
	REPT 4
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,PATTERN_HEIGHT
	mov	DI,BX
	cmp	DI,(SCREEN_HEIGHT-PATTERN_HEIGHT+1)*SCREEN_XBYTES
	jb	short ODD_YLOOP2
IF WIDE_RANGE ne 0
	jmp	short ODD_YLOOP1_ENTRY
ENDIF

	EVEN
ODD_YLOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AL
	xor	AL,DH
	xchg	AH,AL
	mov	ES:[DI+1],AX
	add	DI,SCREEN_XBYTES

	dec	CH
IF WIDE_RANGE ne 0
	jz	short ODD_YLOOP_END
ODD_YLOOP1_ENTRY:
ENDIF
	cmp	DI,SCREEN_HEIGHT*SCREEN_XBYTES
	jb	short ODD_YLOOP1

	sub	DI,SCREEN_HEIGHT*SCREEN_XBYTES

	EVEN
ODD_YLOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AL
	xor	AL,DH
	xchg	AH,AL
	mov	ES:[DI+1],AX
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short ODD_YLOOP2

ODD_YLOOP_END:
	lodsw
	cmp	AL,80h
	je	short ODD_COLOR_LOOP

	; ����ȃf�[�^�Ȃ�� AL=0������c(�Ђ�)
	out	GC_MODEREG,AL		; grcg off
	MRETURN
endfunc			; }

END
