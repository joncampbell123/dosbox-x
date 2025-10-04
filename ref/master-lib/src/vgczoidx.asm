; master library - VGA 16color - trapezoid - noclip
;
; Description:
;	��`�h��Ԃ�(ES:�`���Ώ�)
;
; Subroutines:
;	vgc_draw_trapezoidx
;
; Variables:
;	trapez_a, trapez_b	��`�`���ƕϐ�
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
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;	�E���炩���ߐF�≉�Z���[�h�� vgc_setcolor()�Ŏw�肵�Ă��������B
;	�E�N���b�s���O�͈�؂��܂���B
;	�E���ӓ��m���������Ă���ƁA���������`������܂��B
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
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
;	92/7/10 �N���b�s���O���������ӌ���������(koizoidx.asm)
;	92/7/14 ����
;	92/7/17 �ŏ��̓_������x���ƕςɂȂ��Ă�bug��fix...����(;_;)
;		���x�́A���� 1dot�̕��s�Ȑ}�`�͂������x����
;	93/3/2 ����(>_<)<-�Ӗ��Ȃ�
;	93/ 5/29 [M0.18] .CONST->.DATA
;	94/ 2/23 [M0.23] make_linework��EXTRN���폜(���ꂾ��(^^;)
;	94/ 4/ 9 Initial: vgczoidx.asm/master.lib 0.23

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

	; ��`�`��p�ϐ�
	EXTRN trapez_a: WORD, trapez_b: WORD
	EXTRN graph_VramWidth: WORD

	.CODE
	include vgc.inc

;-------------------------------------------------------------------------
; draw_trapezoidx - ��`�̕`�� �����i�@�\�팸�j��
; IN:
;	SI : unsigned yadr	; �`�悷��O�p�`�̏�̃��C���̍��[��VRAM�̾��
;	DX : unsigned ylen	; �`�悷��O�p�`�̏㉺�̃��C����(y2-y1)
;	ES : unsigned gseg	; �`�悷��VRAM�̃Z�O�����g(ClipYT_seg)
;	trapez_a		; ����a��x�̏����l�ƌX��
;	trapez_b		; ����b��x�̏����l�ƌX��
; BREAKS:
;	AX,BX,CX,DX,SI,DI,flags
;
	public vgc_draw_trapezoidx
vgc_draw_trapezoidx	PROC NEAR
	push	BP
	mov	AX,graph_VramWidth
	mov	CS:add_width,AX

	mov	AX,[trapez_a+d]
	mov	CS:trapez_a_d,AX
	mov	AX,[trapez_b+d]
	mov	CS:trapez_b_d,AX

	mov	AX,[trapez_a+dlx]
	mov	CS:trapez_a_dlx,AX
	mov	AX,[trapez_b+dlx]
	mov	CS:trapez_b_dlx,AX

	jmp	$+2		; break pipeline

	mov	BP,[trapez_a+x]
	mov	CX,[trapez_b+x]

	EVEN
YLOOP:
	; ������ (without clipping) start ===================================
	; IN: SI... x=0�̎���VRAM ADDR(y*80)	BP,CX... ���x���W
	mov	BX,BP

	sub	CX,BX		; CX := bitlen
				; BX := left-x
	sbb	AX,AX
	xor	CX,AX
	sub	CX,AX
	and	AX,CX
	sub	BX,AX

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
	; ������ (without clipping) end ===================================

	add	[trapez_b+s],1234h
	org $-2
trapez_b_dlx	dw ?
	mov	CX,[trapez_b+x]
	adc	CX,1234h
	org $-2
trapez_b_d	dw ?
	mov	[trapez_b+x],CX

	add	[trapez_a+s],1234h
	org $-2
trapez_a_dlx	dw ?
	adc	BP,1234h
	org $-2
trapez_a_d	dw ?

	add	SI,1234h		; yadr
	org $-2
add_width	dw ?
	dec	DX			; ylen
	jns	short YLOOP

	mov	[trapez_a+x],BP
	pop	BP
	ret
vgc_draw_trapezoidx	ENDP

END
