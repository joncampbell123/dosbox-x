; superimpose & master library module
;
; Description:
;	BFNT�t�@�C������p���b�g����ǂݎ��APalettes�ɐݒ肷��
;
; Functions/Procedures:
;	int bfnt_palette_set( int handle, BfntHeader * header ) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	�n�[�h�E�F�A�p���b�g�ւ̐ݒ�͍s���܂���B
;	�K�v�ɉ����āApalette_show(), black_in()�Ȃǂ��Ăяo���ĉ������B
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
;
;$Id: bftpalst.asm 0.05 93/02/19 20:07:23 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
	include	super.inc

	.DATA
	EXTRN Palettes : BYTE

	.CODE

func BFNT_PALETTE_SET
	push	BP
	mov	BP,SP
	handle	= (RETSIZE+1+DATASIZE)*2
	header	= (RETSIZE+1)*2

	_push	DS
	_lds	BX,[BP+header]
	test	[BX].col,80h
	_pop	DS
	jz	short INVALID

file_ok:
	mov	AH,3fh
	mov	BX,[BP+handle]
	mov	CX,48			;16�F * 3
	mov	DX,offset Palettes
	int	21h			;read handle
	jc	short INVALID

	; �p���b�g�f�[�^�̕ϊ�
	mov	BX,DX
	mov	CX,1004h
PALLOOP:
	mov	DL,[BX]		; b
	mov	AX,[BX+1]	; r,g
	mov	[BX],AX		; r,g
	mov	[BX+2],DL	; b
	add	BX,3
	dec	CH
	jnz	short PALLOOP

	mov	AX,NoError
	jmp	short OWARI
INVALID:
	stc
	mov	AX,InvalidData
OWARI:
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
