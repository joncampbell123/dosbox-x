PAGE 98,120
; gc_poly library - calc - trapezoid
;
; Subroutines:
;	make_linework
;
; Variables:
;	trapez_a, trapez_b	��`�`���ƕϐ�
;
; Description:
;	��`�`��p�̑��Ӄf�[�^�ݒ�
;
; Binding Target:
;	asm routine
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;
; Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Notes:
;
; Author:
;	���ˏ��F
;
; �֘A�֐�:
;
;
; Revision History:
;	92/3/21 Initial(koizoid.asm)
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
;	92/7/13 make_line.asm�ɕ���

	.186

	.MODEL SMALL

; LINEWORK�\���̂̊e�����o�̒�`
; ����  - �̾�� - ����
x	= 0	; ���݂�x���W
dlx	= 2	; �덷�ϐ��ւ̉��Z�l
s	= 4	; �덷�ϐ�
d	= 6	; �ŏ����ړ���(�����t��)

	.DATA?

	; ��`�`��p�ϐ�
		EXTRN trapez_a: WORD, trapez_b: WORD

	.CODE

;-------------------------------------------------------------------------
; make_linework - DDA LINE�p�\���̂̍쐬
; IN:
;	DS:BX : LINEWORK * w ;	�������ݐ�
;	DX :	int x1 ;	��_��x
;	AX :	int x2 ;	���̓_��x
;	CX :	int y2 - y1 ;	�㉺��y�̍�
;
; BREAKS:
;	AX,CX,DX,Flags
;
	public make_linework
	EVEN
make_linework	PROC NEAR
	push	SI
	mov	[BX+x],DX	; w.x = x1

	sub	AX,DX		; AX = (x2 - x1)
	cmp	CX,1
	adc	CX,0
	cwd
	idiv	CX
	cmp	DX,8000h
	adc	AX,-1
	mov	[BX+d],AX	; w.d = (x2-x1)/(y2-y1)
	cmp	DX,8000h
	cmc
	sbb	SI,SI
	add	DX,SI
	xor	DX,SI
	xor	AX,AX
	div	CX
	add	AX,SI
	xor	AX,SI
	mov	[BX+dlx],AX	; w.dlx = (x2-x1)%(y2-y1)*0x10000L
	mov	[BX+s],8000h	; w.s = 8000h
	pop	SI
	ret
	EVEN
make_linework	ENDP

END
