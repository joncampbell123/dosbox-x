; master library - (pf.lib)
;
; Description:
;	�������݃o�b�t�@�̃t���b�V��
;
; Functions/Procedures:
;	int bflush(bf_t bf);
;
; Parameters:
;	bf	b�t�@�C���n���h��
;
; Returns:
;	0	����
;	-1	���s(diskfull?)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 186
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
;	iR
;	���ˏ��F
;
; Revision History:
; BFLUSH.ASM 1         483 94-05-26   22:28
;	95/ 1/10 Initial: bflush.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func BFLUSH	; bflush() {
	push	BP
	mov	BP,SP
	push	DS

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	mov	DS,[BP+bf]		; BFILE�\���̂̃Z�O�����g

	mov	CX,DS:[b_pos]
	jcxz	short _ok

	mov	BX,DS:[b_hdl]
	lea	DX,DS:[b_buff]
	msdos	WriteHandle
	jc	short _error

	sub	DS:[b_pos],AX
	jnz	short _error

_ok:
	clr	AX
	pop	DS
	pop	BP
	ret	(1)*2

_error:
	mov	AX,EOF
	pop	DS
	pop	BP
	ret	(1)*2
endfunc		; }

END
