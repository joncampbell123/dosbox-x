; superimpose & master library module
;
; Description:
;	�o�^����Ă���p�^�[�����폜����
;
; Functions/Procedures:
;	int super_cancel_pat( int num ) ;
;
; Parameters:
;	num	�폜����p�^�[���ԍ�
;
; Returns:
;	NoError:	(cy=0) ����
;	GeneralFailure:	(cy=1) ���̔ԍ��͓o�^����Ă��Ȃ�
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
;	
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
;$Id: supercan.asm 0.02 93/01/15 11:45:11 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN super_patsize:WORD	; superpa.asm
	EXTRN super_patdata:WORD	; superpa.asm
	EXTRN super_patnum:WORD		; superpa.asm

	.CODE
	EXTRN	HMEM_FREE:CALLMODEL	; memheap.asm

func SUPER_CANCEL_PAT	; super_cancel_pat() {
	mov	BX,SP

	num	= (RETSIZE+0)*2

	xor	DX,DX			;0
	mov	BX,SS:[BX+num]
	cmp	BX,super_patnum
	jae	short error		; �Ō�̃p�^�[������Ȃ�G���[

	mov	CX,BX
	shl	BX,1			;near pointer
	mov	AX,super_patsize[BX]
	or	AX,AX
	jz	short error		; ���������m�ۂ���ĂȂ��Ȃ�G���[
	push	super_patdata[BX]
	call	HMEM_FREE
	mov	super_patdata[BX],DX	;0
	mov	super_patsize[BX],DX	;0
	inc	CX
	cmp	CX,super_patnum
	jne	short skip
	; �Ō�p�^�[�����������̂ŁA�c���������̍Ō�̃p�^�[������������
canceled:
	dec	super_patnum
	jz	short skip
	dec	BX
	dec	BX
	mov	CX,super_patdata[BX]
	jcxz	short canceled
	EVEN
skip:
	mov	AX,NoError
	clc
	ret	2
	EVEN
error:
	stc
	mov	AX,GeneralFailure
	ret	2
endfunc			; }

END
