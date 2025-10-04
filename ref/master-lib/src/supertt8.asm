; superimpose & master library module
;
; Description:
;	�p�^�[���̕\��(8xn����, 4�F�ȓ�)
;
; Functions/Procedures:
;	void super_put_tiny_small( int x, int y, int num ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	���c�h�b�g��n�́A�����łȂ���΂����܂���
;	���炩���߁Asuper_convert_tiny �Ńf�[�^��ϊ����Ă����K�v�A��
;	�f�[�^�`��: (super_patdata[])
;		--------------------------
;		1���[�h = (�ЂƂ߂̐F�R�[�h << 8) + 80h
;		n�o�C�g = �f�[�^
;		--------------------------
;		1���[�h = (�ӂ��߂̐F�R�[�h << 8) + 80h
;		n�o�C�g = �f�[�^
;		�c
;		--------------------------
;		1���[�h = 0 (�I���)
;
;	�F�� 4�F��葽���ƁA�f�[�^�������傫���Ȃ�̂ŕϊ�����Ƃ�����
;
;	�V���[�e�B���O�Q�[���Ȃ񂩂ŁA��ʂɕ\�������u�e�v�́A
;	8xn dot �łł�����?����Ȃ炱��ō����ɂł����
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	����(���ˏ��F)
;
; Revision History:
;	93/ 9/18 Initial: supertt8.asm / master.lib 0.21
;	94/ 8/16 [M0.23] �c�h�b�g�����ς�(�������A�����Ɍ���)
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	super_patdata:WORD
	EXTRN	super_patsize:WORD

SCREEN_XBYTES	equ 80
GC_MODEREG	equ 07ch
GC_TILEREG	equ 07eh
GC_RMW		equ 0c0h ; ���������ޯĂ������Ă����ޯĂ����ڼ޽����珑��

	.CODE

func SUPER_PUT_TINY_SMALL	; super_put_tiny_small() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg
	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+num]
	shl	BX,1
	mov	BP,super_patsize[BX]
	mov	DS,super_patdata[BX]

	mov	AX,DI
	shl	AX,2
	add	DI,AX
	shl	DI,4
	mov	AX,CX
	and	CX,7		; CL = x & 7
	shr	AX,3
	add	DI,AX		; DI = draw start offset

	xor	SI,SI

	lodsw
	cmp	AL,80h
	jne	short RETURN	; �P�F���Ȃ��Ƃ�(��������)

	mov	AL,GC_RMW
	pushf
	CLI
	out	GC_MODEREG,AL
	popf

	push	AX
	mov	DX,BP			; DL=PATTERN_HEIGHT
	mov	AL,SCREEN_XBYTES
	mul	DL
	mov	BP,AX			; BP=PATTERN_HEIGHT * SCREEN_XBYTES
	shr	DL,1			; 2�s�P�ʂŕ`�悷�邩��
	pop	AX

	EVEN
COLOR_LOOP:
	REPT 4
	shr	AH,1	; �F�̎w�肠���
	sbb	AL,AL
	out	GC_TILEREG,AL
	ENDM

	mov	CH,DL
	EVEN
YLOOP:
	lodsw
	mov	BL,AH
	xor	AH,AH
	ror	AX,CL
	mov	ES:[DI],AX

	xor	BH,BH
	ror	BX,CL
	mov	ES:[DI+SCREEN_XBYTES],BX

	add	DI,SCREEN_XBYTES*2
	dec	CH
	jnz	short YLOOP

	sub	DI,BP
	lodsw
	cmp	AL,80H
	je	short COLOR_LOOP

	out	GC_MODEREG,AL		; grcg off

RETURN:
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

END
