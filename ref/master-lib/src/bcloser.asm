; master library - (pf.lib)
;
; Description:
;	�ǂݍ��ݗp�ɊJ�����t�@�C���̃N���[�Y
;
; Functions/Procedures:
;	void bcloser(bf_t bf);
;
; Parameters:
;	bf	b�t�@�C���n���h��
;
; Returns:
;	none
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
; BCLOSER.ASM         339 94-05-26   22:51
;	95/ 1/10 Initial: bcloser.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	HMEM_FREE:CALLMODEL

func BCLOSER	; bcloser() {
	push	BP
	mov	BP,SP

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	mov	ES,[BP+bf]		; BFILE�\���̂̃Z�O�����g
	mov	BX,ES:[b_hdl]
	msdos	CloseHandle

	push	ES
	_call	HMEM_FREE

	pop	BP
	ret	(1)*2
endfunc		; }

END
