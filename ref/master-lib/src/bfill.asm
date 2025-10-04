; master library - (pf.lib)
;
; Description:
;	�ǂݍ��݃o�b�t�@�ւ̏[�U�Ɛ擪�o�C�g�̓ǂݍ���
;
; Functions/Procedures:
;	int bfill(bf_t bf);
;
; Parameters:
;	bf	b�t�@�C���n���h��
;
; Returns:
;	-1	�t�@�C���I�[�ɒB�����̂�1�o�C�g���ǂ߂Ȃ�����
;	0�`255	�o�b�t�@�̐擪�o�C�g(�[�U����)
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
; BFILL.ASM         571 94-05-26   22:44
;	95/ 1/10 Initial: bfill.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func BFILL	; bfill() {
	push	BP
	mov	BP,SP
	push	DS

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	mov	DS,[BP+bf]		; BFILE�\���̂̃Z�O�����g

	mov	BX,DS:[b_hdl]
	mov	CX,DS:[b_siz]
	lea	DX,DS:[b_buff]
	msdos	ReadHandle
	jc	short _error
	chk	AX
	jz	short _error

_getc:
	dec	AX
	mov	DS:[b_left],AX
	mov	DS:[b_pos],1
	mov	AL,DS:[b_buff]
	clr	AH

	pop	DS
	pop	BP
	ret	(1)*2

_error:
	clr	AX
	mov	DS:[b_left],AX
	dec	AX		; mov	AX,EOF

	pop	DS
	pop	BP
	ret	(1)*2
endfunc		; }

END
