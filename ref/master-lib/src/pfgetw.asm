; master library - (pf.lib)
;
; Description:
;	2�o�C�g�����̓ǂݍ���
;
; Functions/Procedures:
;	int pfgetw(pf_t pf)
;
; Parameters:
;	pf	pf�t�@�C���n���h��
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
; PFGETW.ASM         345 94-09-17   22:53
;	95/ 1/10 Initial: pfgetw.asm/master.lib 0.23
;	95/ 1/19 MODIFY: ����1�o�C�g�ǂݍ��݃��[�`�������W�X�^�n���ɕύX

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func PFGETW	; pfgetw() {
	push	BP
	mov	BP,SP

	;arg	pf:word
	pf	= (RETSIZE+1)*2

	mov	ES,[BP+pf]
	call	ES:[pf_getc]

	push	AX
	call	ES:[pf_getc]
	mov	AH,AL
	pop	BX
	mov	AL,BL

	pop	BP
	ret	(1)*2
endfunc	; }

END
