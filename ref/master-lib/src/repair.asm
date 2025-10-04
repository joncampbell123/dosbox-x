; superimpose & master library module - PC-9801 - CINT
;
; Description:
;	�w����W�A�w��L�����N�^�̑傫���̎l�p�`��page 1����page 0�ɓ]������
;
; Functions/Procedures:
;	void repair_back( int x, int y, int num ) ;
;
; Parameters:
;	x,y	����[�̍��W
;	num	�傫�����擾����L�����N�^�ԍ�
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
;
; Notes:
;	���s��A�A�N�Z�X�y�[�W�� page 0�ɂȂ�܂��B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: repair.asm 0.09 92/05/29 20:10:58 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 3 [M0.16] ����
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_buffer:WORD

	.CODE

func REPAIR_BACK
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	DX,[BP+x]
	mov	AX,[BP+y]
	mov	BX,[BP+num]
	mov	SI,AX		;-+
	shl	SI,2		; |SI=y*80
	add	SI,AX		; |
	shl	SI,4		;-+
	shr	DX,3		;AX=x/8
	add	SI,DX		;GVRAM offset address
	mov	CS:_SI1_,SI
	mov	CS:_SI2_,SI
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	mov	CL,DH
	mov	CH,0
	inc	CX
	mov	BP,80
	sub	BP,CX
	mov	ES,super_buffer
	shr	CX,1
	mov	BL,CL
	sbb	BH,BH
	mov	AX,0a800h
	call	BACK
	mov	AX,0b000h
	call	BACK
	mov	AX,0b800h
	call	BACK
	mov	AX,0e000h
	call	BACK

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc

; �]���T�u���[�`��
; In:
;	AX	�]��VRAM�Z�O�����g
;	DL	�c��line��
;	BL	����word��
;	BH	����word�ɓ���Ȃ�(�)�o�C�g�̗L��(0ffh=����,00h=�Ȃ�)
;	BP	80-���̃o�C�g��
;	ES	�]���o�b�t�@�Z�O�����g

	EVEN
BACK	proc	near
	mov	DS,AX
	inc	AX
	out	0a6h,AL

	JMOV	SI,_SI1_
	mov	DH,DL
	mov	DI,0
	ror	BH,1
	jnb	short BACK_EVEN

	EVEN
BACK_ODD:
	movsb
	rep	movsw
	add	SI,BP
	mov	CL,BL
	dec	DH
	jnz	BACK_ODD
	jmp	short REPAIR
	EVEN

BACK_EVEN:
	rep	movsw
	add	SI,BP
	mov	CL,BL
	dec	DH
	jnz	short BACK_EVEN
	EVEN

REPAIR:
	mov	AX,DS	; ES<->ES
	mov	DI,ES
	mov	DS,DI
	mov	ES,AX

	JMOV	DI,_SI2_
	mov	DH,DL
	mov	SI,0
	out	0a6h,AL
	ror	BH,1
	jnb	short REPAIR_EVEN
	EVEN
REPAIR_ODD:
	movsb
	rep	movsw
	add	DI,BP
	mov	CL,BL
	dec	DH
	jnz	short REPAIR_ODD

	mov	AX,DS
	mov	ES,AX
	ret

	EVEN
REPAIR_EVEN:
	rep	movsw
	add	DI,BP
	mov	CL,BL
	dec	DH
	jnz	short REPAIR_EVEN

	mov	AX,DS
	mov	ES,AX
	ret
BACK	endp

END
