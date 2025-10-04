; master library - (pf.lib)
;
; Description:
;	�������ݗp�Ƀt�@�C�����J��
;
; Functions/Procedures:
;	bf_t bopenw(const char *fname);
;
; Parameters:
;	fname	�t�@�C����
;
; Returns:
;	0	�G���[�Bpferrno�Ɏ�ʂ�����B
;		PFENOTOPEN	�t�@�C�����J���Ȃ�
;		PFENOMEM	�������s��
;
;	0�ȊO	b*�n�t�@�C���֐��ɓn���n���h��
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
; BOPENW.ASM         736 94-05-30   22:10
;	95/ 1/10 Initial: bopenw.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�

	.186
	.model SMALL
	include func.inc
	include pf.inc
	include super.inc

	.DATA
	EXTRN	pferrno:BYTE
	EXTRN	bbufsiz:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	DOS_CREATE:CALLMODEL
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

func BOPENW	; bopenw() {
	push	BP
	mov	BP,SP

	;arg	fname:dataptr
	fname	= (RETSIZE+1)*2

	mov	mem_AllocID,MEMID_bfile
	mov	AX,bbufsiz
	add	AX,size BFILE
	push	AX
	_call	HMEM_ALLOCBYTE
	jc	short _nomem

	mov	ES,AX			; BFILE�\���̂̃Z�O�����g

	_push	[BP+fname+2]
	push	[BP+fname]
	push	0			; �t�@�C������(normal)
	_call	DOS_CREATE
	jc	short _cantopen

	mov	ES:[b_hdl],AX
	mov	ES:[b_pos],0
	mov	AX,bbufsiz
	mov	ES:[b_siz],AX

	mov	AX,ES
	pop	BP
	ret	(DATASIZE)*2

_nomem:
	mov	pferrno,PFENOMEM
	jmp	short _error
_cantopen:
	push	ES
	_call	HMEM_FREE
	mov	pferrno,PFENOTOPEN
_error:
	clr	AX
	pop	BP
	ret	(DATASIZE)*2
endfunc		; }

END
