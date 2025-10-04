; superimpose & master library module
;
; Description:
;	�w��ԍ��͈͂̃p�^�[�������ׂĊJ������
;
; Function/Procedures:
;	void super_clean( int min_pat, int max_pat ) ;
;
; Parameters:
;	min_pat	��������ŏ��p�^�[���ԍ�
;	max_pat	��������ő�p�^�[���ԍ�
;
; Returns:
;	none
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
;	min_pat > max_pat ���Ɖ������܂���B
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
;	93/12/25 Initial: supercln.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patnum:WORD

	.CODE
	EXTRN	SUPER_CANCEL_PAT:CALLMODEL


func SUPER_CLEAN ; super_clean() {
	push	SI
	mov	SI,SP
	push	DI

	; ����
	min_pat	= (RETSIZE+2)*2
	max_pat	= (RETSIZE+1)*2

	mov	DI,SS:[SI+max_pat]
	cmp	DI,super_patnum
	jl	short MAX_OK
	mov	DI,super_patnum
	dec	DI
MAX_OK:
	mov	SI,SS:[SI+min_pat]
	cmp	SI,DI
	ja	short RETURN
C_LOOP:
	push	SI
	_call	SUPER_CANCEL_PAT
SKIP:
	inc	SI
	cmp	SI,DI
	jbe	short C_LOOP
RETURN:
	pop	DI
	pop	SI
	ret	4
endfunc		; }

END
