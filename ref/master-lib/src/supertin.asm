; superimpose & master library module
;
; Description:
;	16�F�p�^�[���� 4�F�ȓ��f�[�^�ɕϊ�����(super_put_tiny�p)
;
; Function/Procedures:
;	int super_convert_tiny( int num ) ;
;
; Parameters:
;	num	�p�^�[���ԍ�
;
; Returns:
;	InvalidData	(cy=1) num ���o�^���ꂽ�p�^�[���ł͂Ȃ�
;	InvalidData	(cy=1) �F��5�F�ȏ゠��(�p�^�[���͕s��)
;	NoError		(cy=0) ����           (�p�^�[���͕ϊ����ꂽ)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�Ȍ�A���̃p�^�[���ԍ��� super_put�Ȃǂ�16�F�n�ŕ\�����Ă�
;	�����Ⴎ����ɂȂ��Ă܂���B
;	�p�^�[���̑傫���̓`�F�b�N���Ă܂ւ�B
;	super_put_tiny�� 16xn�p�^�[����p, 
;	super_put_tiny_small 8xn��p�Ȃ̂Œ��ӂ��Ƃ��Ă�
;	�}�X�N�p�^�[���͂�������Ή����Ă܁[��
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 9/18 Initial: supertin.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patdata:WORD
	EXTRN	super_patsize:WORD
	EXTRN	super_buffer:WORD
	EXTRN	super_patnum:WORD

	.CODE

MAX_COLOR equ 4

func SUPER_CONVERT_TINY ; super_convert_tiny() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	xor	DI,DI
	mov	AX,MAX_COLOR
	push	AX		; stack top = �J�E���^

	; ����
	num	= (RETSIZE+1)*2
	mov	BX,[BP+num]
	cmp	BX,super_patnum		; �p�^�[���ԍ��ُ�
	jae	short NO_PATTERN
	shl	BX,1

	mov	CX,super_patdata[BX]
	jcxz	short NO_PATTERN	; �p�^�[���ԍ��ُ�

	mov	AX,super_patsize[BX]
	mul	AH
	mov	BP,AX		; BP = patsize

	mov	ES,super_buffer

	mov	DS,CX
	mov	BH,0ffh		; bh = color

COLORLOOP:
	xor	SI,SI
	mov	AX,BX
	not	AX
	mov	AL,80h
	and	AH,0fh
	stosw
	mov	CX,BP
	shr	CX,1
	rep	movsw		; mask
	sub	DI,BP

	mov	BL,4	; number of bit
	mov	CX,BP
	shr	CX,1

	EVEN
BITLOOP:
	ror	BH,1
	sbb	DX,DX	; �Ђ����肩�����Ă�̂�, BH�� 15->0�ɐi��ł邯��
			; �e�X�g�� 0->15�̏��ɍs���Ă�킯
			; �Ȃ�cmc�����Ȃ��̂��Ƃ����ƁA�����o�C�g��
			; ���߂����낦�邽�߂�
	; in: CX = BP / 2
ANDLOOP:
	lodsw
	xor	AX,DX
	and	ES:[DI],AX
	inc	DI
	inc	DI
	loop	short ANDLOOP

	sub	DI,BP
	mov	CX,BP
	shr	CX,1
	dec	BL
	jnz	short BITLOOP

	lea	DX,[DI-2]
	xor	AX,AX
	repe	scasw
	mov	DI,DX
	jz	short NEXTCOLOR
	lea	DI,[DI+BP+2]
	pop	AX		; �J�E���^
	dec	AX
	push	AX		; �J�E���^
	js	short TOOMANY_COLORS
NEXTCOLOR:
	sub	BH,11H
	jnc	short COLORLOOP

	; �����߂�
	mov	CX,DI
	shr	CX,1
	push	DS	; DS <-> ES
	push	ES
	pop	DS
	pop	ES
	xor	AX,AX
	mov	DI,AX
	mov	SI,AX
	rep	movsw
	stosw		; terminator

	clc
	mov	AX,NoError
	jmp	short RETURN

TOOMANY_COLORS:
NO_PATTERN:
	mov	AX,InvalidData
	stc
	;jmp	short RETURN

RETURN:
	pop	DI	; �J�E���^���̂Ă�

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	2
endfunc		; }

END
