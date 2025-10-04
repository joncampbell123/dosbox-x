; superimpose & master library module
;
; Description:
;	���ׂẴp�^�[���^�L�����N�^�̊J��
;
; Function/Procedures:
;	void super_free(void) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
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
;	93/ 9/16 Initial: superfre.asm/master.lib 0.21
;	93/12/ 6 [M0.22] ���łɊJ������Ă���̂ɂɌĂяo���ꂽ���̃`�F�b�N
;	95/ 4/ 7 [M0.22k] super_charfree�ɂ���ăL�����N�^���J������

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN super_patdata:WORD	; superpa.asm
	EXTRN super_patsize:WORD	; superpa.asm
	EXTRN super_buffer:WORD		; superpa.asm
	EXTRN super_patnum:WORD		; superpa.asm

	EXTRN super_charfree:WORD	; superpa.asm

	.CODE
	EXTRN HMEM_FREE:CALLMODEL
	EXTRN SUPER_CANCEL_PAT:CALLMODEL

func SUPER_FREE	; super_free() {
	cmp	super_buffer,0		; super_buffer��0�Ȃ�����S��
	je	short SKIP		; �J������Ă���Ɣ��f
	push	super_buffer
	_call	HMEM_FREE
	mov	super_buffer,0

	jmp	short FREESTART
PATFREE:
	dec	AX
	push	AX
	_call	SUPER_CANCEL_PAT

FREESTART:
	mov	AX,super_patnum
	test	AX,AX
	jnz	short PATFREE

	cmp	super_charfree,0
	je	short SKIP
	call	word ptr super_charfree
SKIP:
	ret
endfunc		; }

END
