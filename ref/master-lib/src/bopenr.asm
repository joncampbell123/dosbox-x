; master library - (pf.lib)
;
; Description:
;	�t�@�C�����J��
;
; Functions/Procedures:
;	bf_t bopenr(const char *fname);
;
; Parameters:
;	fname
;
; Returns:
;	0	�G���[�Bpferrno�Ɏ�ʂ��i�[�����B
;		PFENOMEM	�������s��
;		PFENOTOPEN	�t�@�C�����J���Ȃ�

;	0�ȊO	�����B�Ȍケ�̒l�� b*�t�@�C���֐��Q�ɓn�����ƁB
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
; BOPENR.ASM 1         729 94-06-05   13:01
;	95/ 1/10 Initial: bopenr.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�


	.model SMALL
	include func.inc
	include pf.inc
	include super.inc

	.DATA
	EXTRN	pferrno:BYTE
	EXTRN	bbufsiz:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	DOS_ROPEN:CALLMODEL
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

func BOPENR	; bopenr() {
	push	BP
	mov	BP,SP

	;arg	fname:dataptr
	fname = (RETSIZE+1)*2

	mov	mem_AllocID,MEMID_bfile
	mov	AX,bbufsiz
	add	AX,size BFILE
	push	AX
	_call	HMEM_ALLOCBYTE
	jc	short _nomem

	mov	ES,AX			; BFILE�\���̂̃Z�O�����g

	_push	[BP+fname+2]
	push	[BP+fname]
	_call	DOS_ROPEN
	jc	short _cantopen

	mov	ES:[b_hdl],AX
	mov	ES:[b_left],0
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
