; master library - VGA 16color - trapezoid
;
; Description:
;	��`�h��Ԃ�(grc_setclip()�Őݒ肵���̈悪�Ώ�)
;
; Subroutines:
;	vgc_draw_trapezoid
;
; Variables:
;	trapez_a, trapez_b	��`�`���ƕϐ�
;
; Returns:
;	
;
; Binding Target:
;	asm routine
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
;	�@�����W���㋫�E�ɂ������Ă���ƒx���Ȃ�܂��B
;	�@�i�㋫�E�Ƃ̌�_���v�Z�����ɁA�ォ�珇�ɒ��ׂĂ��邽�߁j
;	�E���ӓ��m���������Ă���ꍇ�A�˂��ꂽ��`�i�Q�̎O�p�`�����_��
;	�@�ڂ��Ă����ԁj��`�悵�܂��B
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
;	94/ 4/ 9 Initial: vgczoid.asm/master.lib 0.23

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

	EXTRN	ClipXL:WORD, ClipXW:WORD
	EXTRN	ClipYT:WORD, ClipYB:WORD, ClipYB_adr:WORD
	EXTRN	graph_VramWidth:WORD

	; ��`�`��p�ϐ�
	EXTRN trapez_a: WORD, trapez_b: WORD

	.CODE
	include func.inc
	include vgc.inc

;-------------------------------------------------------------------------
; vgc_draw_trapezoid - ��`�̕`��
; IN:
;	SI : unsigned yadr	; �`�悷��O�p�`�̏�̃��C���̍��[��VRAM�̾��
;	DX : unsigned ylen	; �`�悷��O�p�`�̏㉺�̃��C����(y2-y1)
;	ES : unsigned gseg	; �`�悷��VRAM�̃Z�O�����g(graph_VramSeg)
;	trapez_a		; ����a��x�̏����l�ƌX��
;	trapez_b		; ����b��x�̏����l�ƌX��
;	ClipXL			; �`�悷��VRAM�̍����؂藎�Ƃ� x ���W
;	ClipXW			; ClipXL�ƁA�E�� x ���W�̍�(����)
; BREAKS:
;	AX,BX,CX,DX,SI,DI,flags
;
	public vgc_draw_trapezoid
vgc_draw_trapezoid	PROC NEAR
	mov	BX,DX
	mov	AX,graph_VramWidth
	mov	CS:vwidth,AX
	imul	ClipYT
	mov	CS:yt_adr,AX
	add	AX,ClipYB_adr
	mov	CS:yb_adr,AX
	mov	DX,BX

	mov	AX,[trapez_a+d]
	mov	CS:trapez_a_d,AX
	mov	AX,[trapez_b+d]
	mov	CS:trapez_b_d,AX

	mov	AX,[trapez_a+dlx]
	mov	CS:trapez_a_dlx,AX
	mov	AX,[trapez_b+dlx]
	mov	CS:trapez_b_dlx,AX

	mov	AX,ClipXL
	mov	CS:clipxl_1,AX
	mov	CS:clipxl_2,AX
	mov	AX,ClipXW
	mov	CS:clipxw_1,AX

	jmp	short $+2	; break pipeline

	push	BP

	mov	CX,[trapez_a+x]
	mov	BP,[trapez_b+x]

YLOOP:
	; ������ (with clipping) start ===================================
	; IN: SI... x=0�̎���VRAM ADDR(y*80)	BP,CX... ���x���W
	cmp	SI,1234h
	org $-2
yt_adr dw ?
	jb	short SKIP_HLINE ; y���͈͊O �� skip
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

	JMOV	DI,clipxw_1
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
	mov	DI,BX		; addr := yaddr + xl div 8
	shr	DI,3
	add	DI,SI

	and	BX,7		; BX := xl and 7
	add	CX,BX
	shl	BX,1
	mov	AL,byte ptr [EDGES+BX]	; ���G�b�W
	not	AX

	mov	BX,CX		;
	and	BX,7
	shl	BX,1

	shr	CX,3
	jz	short LASTW

	test	ES:[DI],AL
	stosb

	mov	AL,0ffh
	dec	CX
	jz	short REPSTOSB2
REPSTOSB:
	or	ES:[DI],AL
	inc	DI
	loop	short REPSTOSB
REPSTOSB2:
LASTW:	and	AL,byte ptr [EDGES+2+BX]	; �E�G�b�W
	test	ES:[DI],AL
	mov	ES:[DI],AL

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

	add	SI,1234h		; yadr
	org $-2
vwidth	dw	?
	dec	DX			; ylen
	js	short OWARI
	jmp	YLOOP		; ������(;_;)
OWARI:
	mov	[trapez_b+x],BP
	pop	BP
	ret
	EVEN
vgc_draw_trapezoid	ENDP
END
