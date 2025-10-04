; master library - (pf.lib)
;
; Description:
;	1�o�C�g�ǂݍ���
;
; Functions/Procedures:
;	int bgetc(bf_t bf);
;
; Parameters:
;	bf	bfile�n���h��
;
; Returns:
;	
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
; BGETC.ASM 1         435 94-05-26   21:51
;	95/ 1/10 Initial: bgetc.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BFILL:CALLMODEL

func BGETC	; bgetc() {
	push	BP
	mov	BP,SP

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	mov	ES,[BP+bf]		; BFILE�\���̂̃Z�O�����g

	cmp	ES:[b_left],0
	jz	short _fill
	dec	ES:[b_left]
	mov	BX,ES:[b_pos]
	inc	ES:[b_pos]
	mov	AL,ES:[b_buff][BX]
	clr	AH
	pop	BP
	ret	(1)*2

_fill:
	push	ES
	_call	BFILL

	pop	BP
	ret	(1)*2
endfunc		; }

END
