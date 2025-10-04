; superimpose & master library module
;
; Description:
;	BFNT+�t�@�C���̃p���b�g����ǂݔ�΂�
;
; Functions/Procedures:
;	int bfnt_palette_skip( int handle, BfntHeader * header ) ;
;
; Parameters:
;	handle	BFNT+�t�@�C���̃t�@�C���n���h��
;	header	BFNT+�t�@�C���̃w�b�_
;
; Returns:
;	NoError		����
;	InvalidData	�p���b�g��񂪑��݂��Ȃ�
;	InvalidData	�t�@�C���|�C���^�̈ړ����ł��Ȃ�
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
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
; $Id: bftpalsk.asm 0.03 93/01/15 11:38:25 Kazumi Rel $
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.MODEL SMALL
	include func.inc
	include	super.inc

	.CODE

MRETURN macro
	pop	BP
	ret	(1+DATASIZE)*2
	EVEN
	endm

func BFNT_PALETTE_SKIP
	push	BP
	mov	BP,SP

	; ����
	handle	= (RETSIZE+1+DATASIZE)*2
	header	= (RETSIZE+1)*2

	_push	DS
	_lds	BX,[BP+header]
	mov	CL,(bfnt_header PTR [BX]).col
	_pop	DS
	test	CL,80h
	jz	short DAME
	mov	AX,1
	and	CL,7fh
	inc	CL
	shl	AX,CL			;color
	mov	DX,AX
	add	DX,AX
	add	DX,AX			;color * 3
	xor	CX,CX
	mov	BX,[BP+handle]
	mov	AX,4201h
	int	21h			;seek
	jc	short DAME
	mov	AX,NoError
	MRETURN
DAME:
	mov	AX,InvalidData
	stc
	MRETURN
endfunc

END
