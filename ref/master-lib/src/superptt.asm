; superimpose & master library module
;
; Description:
;	�p�^�[���̕\��(��16dot����, 4�F�ȓ�)
;
; Functions/Procedures:
;	void super_put_tiny( int x, int y, int num ) ;
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
;	���c�h�b�g��=n
;	���炩���߁Asuper_convert_tiny �Ńf�[�^��ϊ����Ă����K�v�A��
;	�f�[�^�`��: (super_patdata[])
;		--------------------------
;		1���[�h = (�ЂƂ߂̐F�R�[�h << 8) + 80h
;		n*2�o�C�g = �f�[�^
;		--------------------------
;		1���[�h = (�ӂ��߂̐F�R�[�h << 8) + 80h
;		n*2�o�C�g = �f�[�^
;		�c
;		--------------------------
;		1���[�h = 0 (�I���)
;
;	�F�� 4�F��葽���ƁA�f�[�^�������傫���Ȃ�̂ŕϊ�����Ƃ�����
;
;	�V���[�e�B���O�Q�[���Ȃ񂩂ŁA��ʂɕ\�������u�e�v�́A
;	16x n dot �ȓ��łł�����?����Ȃ炱��ō����ɂł����
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	93/ 9/18 Initial: superptt.asm / master.lib 0.21
;	94/ 8/16 [M0.23] �c�h�b�g�����ς�
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	super_patsize:WORD
	EXTRN	super_patdata:WORD

SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; ���������ޯĂ������Ă����ޯĂ����ڼ޽����珑��

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
	endm

func SUPER_PUT_TINY	; super_put_tiny() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg
	mov	BX,[BP+num]
	mov	CX,[BP+x]
	shl	BX,1
	mov	DI,[BP+y]
	mov	BP,super_patsize[BX]
	mov	DS,super_patdata[BX]

	mov	DX,DI
	shl	DI,2
	add	DI,DX
	shl	DI,4
	mov	DX,CX
	and	CX,7		; CL = x & 7
	shr	DX,3
	add	DI,DX		; DI = draw start offset

	xor	SI,SI

	lodsw
	cmp	AL,80h
	jnz	short RETURN	; �P�F���Ȃ��Ƃ�(��������)

	mov	AL,GC_RMW
	pushf
	CLI
	out	GC_MODEREG,AL
	popf

	mov	DL,0ffh		; DL = ���[�h���E�}�X�N
	shr	DL,CL

	mov	BX,BP			; BL=PATTERN_HEIGHT
	mov	BP,AX
	mov	AL,SCREEN_XBYTES
	mul	BL
	xchg	BP,AX			; BP=PATTERN_HEIGHT * SCREEN_XBYTES

	test	DI,1
	jnz	short ODD_COLOR_LOOP
	EVEN
	; �����A�h���X
EVEN_COLOR_LOOP:
	shr	AH,1	; �F�̎w�肠���
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL

	mov	CH,BL
	EVEN
EVEN_YLOOP:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,DL
	mov	ES:[DI],AX
	xor	AL,DH
	mov	ES:[DI+2],AL
	add	DI,SCREEN_XBYTES

	dec	CH
	jnz	short EVEN_YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80h
	je	short EVEN_COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off

RETURN:
	MRETURN

	EVEN
	; ��A�h���X
ODD_COLOR_LOOP:
	shr	AH,1	; �F�̎w�肠���
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL
	shr	AH,1
	sbb	AL,AL
	out	GC_TILEREG,AL

	mov	CH,BL
	EVEN
ODD_YLOOP:
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
	jnz	short ODD_YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80h
	je	short ODD_COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off
	MRETURN
endfunc			; }

END
