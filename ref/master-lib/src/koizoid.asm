PAGE 98,120
; gc_poly library - grcg - trapezoid - PC98V
;
; Subroutines:
;	draw_trapezoid
;
; Variables:
;	trapez_a, trapez_b	��`�`���ƕϐ�
;
; Description:
;	��`�h��Ԃ�(grc_setclip()�Őݒ肵���̈悪�Ώ�)
;
; Binding Target:
;	asm routine
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GRAPHIC CHARGER
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E�O���t�B�b�N��ʂ̐v���[���ɂ̂ݕ`�悵�܂��B
;	�E�F������ɂ́A�O���t�B�b�N�`���[�W���[�𗘗p���Ă��������B
;	�Egrc_setclip()�ɂ��N���b�s���O�ɑΉ����Ă��܂��B
;	�@�����W���㋫�E�ɂ������Ă���ƒx���Ȃ�܂��B
;	�@�i�㋫�E�Ƃ̌�_���v�Z�����ɁA�ォ�珇�ɒ��ׂĂ��邽�߁j
;	�E���ӓ��m���������Ă���ꍇ�A�˂��ꂽ��`�i�Q�̎O�p�`�����_��
;	�@�ڂ��Ă����ԁj��`�悵�܂��B
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
;	grc_setclip()
;
; Revision History:
;	92/3/21 Initial
;	92/3/29 bug fix, ��`�ɑ��ӂ̌�����F�߂�悤�ɂ���
;	92/4/2 ���X����
;	92/4/18 �O�p�`ٰ�݂��番���B�د��ݸޕt���B
;	92/4/19 ���X����
;	92/5/7 ���ȏ��������ɂ�����
;	92/5/20 �C�Ӹد��ݸނ̉��ɂ����Ή��B:-)
;	92/5/22 ����������ȏ��������`
;	92/6/4	grcg_trapezoid()�֐���gc_zoid.asm�ɕ����B
;	92/6/5	bugfix(make_linework��CX��0�̎���div 0���)
;	92/6/5	���[�N���b�v�Ή�
;	92/6/12 �����`
;	92/6/16 TASM�ɑΉ�
;	92/7/13 make_lin.asm�𕪗�
;	93/ 5/29 [M0.18] .CONST->.DATA
;	94/ 2/23 [M0.23] make_linework��EXTRN���폜(���ꂾ��(^^;)

	.186

	.MODEL SMALL

; LINEWORK�\���̂̊e�����o�̒�`
; ����  - �̾�� - ����
x	= 0	; ���݂�x���W
dlx	= 2	; �덷�ϐ��ւ̉��Z�l
s	= 4	; �덷�ϐ�
d	= 6	; �ŏ����ړ���(�����t��)

	.DATA

	EXTRN	EDGES:WORD

	.DATA?

	EXTRN	ClipXL:WORD, ClipXW:WORD, ClipYB_adr:WORD

	; ��`�`��p�ϐ�
	EXTRN trapez_a: WORD, trapez_b: WORD

	.CODE

;-------------------------------------------------------------------------
; draw_trapezoid - ��`�̕`��
; IN:
;	SI : unsigned yadr	; �`�悷��O�p�`�̏�̃��C���̍��[��VRAM�̾��
;	DX : unsigned ylen	; �`�悷��O�p�`�̏㉺�̃��C����(y2-y1)
;	ES : unsigned gseg	; �`�悷��VRAM�̃Z�O�����g(ClipYT_seg)
;	trapez_a		; ����a��x�̏����l�ƌX��
;	trapez_b		; ����b��x�̏����l�ƌX��
;	ClipYB_adr		; �`�悷��VRAM�̍ŉ����W�̍��[�̃A�h���X
;	ClipXL			; �`�悷��VRAM�̍����؂藎�Ƃ� x ���W
;	ClipXW			; ClipXL�ƁA�E�� x ���W�̍�(����)
; BREAKS:
;	AX,BX,CX,DX,SI,DI,flags
;
	public draw_trapezoid
draw_trapezoid	PROC NEAR
	mov	AX,ClipYB_adr
	mov	CS:[yb_adr],AX
	mov	AX,[trapez_a+d]
	mov	CS:[trapez_a_d],AX
	mov	AX,[trapez_b+d]
	mov	CS:[trapez_b_d],AX

	mov	AX,[trapez_a+dlx]
	mov	CS:[trapez_a_dlx],AX
	mov	AX,[trapez_b+dlx]
	mov	CS:[trapez_b_dlx],AX

	mov	AX,ClipXL
	mov	CS:[clipxl_1],AX
	mov	CS:[clipxl_2],AX
	mov	AX,ClipXW
	mov	CS:[clipxw_1],AX

	jmp	short $+2

	push	BP

	mov	CX,[trapez_a+x]
	mov	BP,[trapez_b+x]

YLOOP:
	; ������ (with clipping) start ===================================
	; IN: SI... x=0�̎���VRAM ADDR(y*80)	BP,CX... ���x���W
	cmp	SI,1234h
	org $-2
yb_adr dw ?
	ja	short SKIP_HLINE ; y���͈͊O �� skip

	mov	AX,1234h	; �N���b�v�g���[�����炷
	org $-2
clipxl_1 dw ?			; ClipXL
	sub	CX,AX
	mov	BX,BP
	sub	BX,AX

	test	CX,BX		; x�����Ƀ}�C�i�X�Ȃ�͈͊O
	js	short SKIP_HLINE

	cmp	CX,BX
	jg	short S10
	xchg	CX,BX		; CX�͕K���񕉂ɂȂ�
S10:
	cmp	BH,80h		; if BX < 0 then BX := 0
	sbb	AX,AX
	and	BX,AX

	mov	DI,1234h	; ClipXW
	org $-2
clipxw_1 dw ?
	sub	CX,DI		; if CX >= ClipXW then   CX := ClipXW
	sbb	AX,AX
	and	CX,AX
	add	CX,DI

	sub	CX,BX		; CX := bitlen
				; BX := left-x
	jl	short SKIP_HLINE ; �Ƃ��ɉE�ɔ͈͊O �� skip

	add	BX,1234h	; ClipXL
	org $-2
clipxl_2 dw ?
	mov	DI,BX		; addr := yaddr + xl div $10 * 2
	shr	DI,4
	shl	DI,1
	add	DI,SI

	and	BX,0Fh		; BX := xl and $0F
	add	CX,BX
	sub	CX,16
	shl	BX,1
	mov	AX,[EDGES+BX]	; ���G�b�W
	not	AX

	mov	BX,CX		;
	and	BX,0Fh
	shl	BX,1

	sar	CX,4
	js	short LASTW
	stosw
	mov	AX,0FFFFh
	rep stosw
LASTW:	and	AX,[EDGES+2+BX]	; �E�G�b�W
	stosw
	; ������ (with clipping) end ===================================

SKIP_HLINE:
	add	[trapez_a+s],1234h
	org $-2
trapez_a_dlx	dw ?
	mov	CX,[trapez_a+x]
	adc	CX,1234h
	org $-2
trapez_a_d	dw ?
	mov	[trapez_a+x],CX

	add	[trapez_b+s],1234h
	org $-2
trapez_b_dlx	dw ?
	adc	BP,1234h
	org $-2
trapez_b_d	dw ?

	add	SI,80			; yadr
	dec	DX			; ylen
	js	short OWARI
	jmp	YLOOP		; ������(;_;)
OWARI:
	mov	[trapez_b+x],BP
	pop	BP
	ret
	EVEN
draw_trapezoid	ENDP
END
