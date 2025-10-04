; master library - BFNT
;
; Description:
;	BFNT�w�b�_�����̃t�@�C������̓ǂݍ���
;
; Functions/Procedures:
;	int bfnt_header_load(const char *filename, BfntHeader *header);
;
; Parameters:
;	filename	BFNT/BFNT+�t�@�C����
;	header		�ǂݍ��ݐ�
;
; Returns:
;	NoError		����
;	FileNotFound	�t�@�C�����J���Ȃ�
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
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
;	Kazumi(���c  �m)
;
; Revision History:
;	95/ 1/31 Initial: bfnthead.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc
	include super.inc

	EXTRN DOS_ROPEN:CALLMODEL

	.CODE

MRETURN macro
	_pop	DS
	pop	BP
	ret	(DATASIZE*2)*2
	EVEN
	endm

func BFNT_HEADER_LOAD	; bfnt_header_load() {
	push	BP
	mov	BP,SP
	_push	DS
	filename = (RETSIZE+DATASIZE+1)*2
	header	= (RETSIZE+1)*2

	_push	[BP+filename+2]
	push	[BP+filename]
	call	DOS_ROPEN
	jc	short OPEN_ERROR
	mov	BX,AX
	_lds	DX,[BP+header]
	mov	AH,3fh			;read handle
	mov	CX,BFNT_HEADER_SIZE
	int	21h
	mov	AH,3eh
	int	21h			;close handle
	mov	AX,NoError

OPEN_ERROR:
	MRETURN
endfunc			; }

END
