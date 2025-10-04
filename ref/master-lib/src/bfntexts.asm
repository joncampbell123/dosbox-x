; superimpose & master library module
;
; Description:
;	BFNT�t�@�C���̃f�[�^���̐擪�փV�[�N����
;
; Functions/Procedures:
;	int bfnt_extend_header_skip( unsigned handle, BFNT_HEADER * bfntheader )
;
; Parameters:
;	handle		�t�@�C���n���h��
;	bfntheader	BFNT�w�b�_�\���̂ւ̃|�C���^
;
; Returns:
;	NoError		(cy=0)	�ǂݎ�萬��
;	InvalidData	(cy=1)	�ǂݎ�莸�s
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
;	$Id: bfntexts.asm 0.05 93/01/19 17:05:57 Kazumi Rel $
;
;	93/ 2/25 Initial: master.lib <- super.lib 0.22b
;	94/ 6/10 [M0.23] seek �� current���΂ɕύX
;

	.MODEL SMALL
	include func.inc
	include	super.inc

	.CODE

func BFNT_EXTEND_HEADER_SKIP
	mov	BX,SP

	; ����
	handle	   = (RETSIZE+DATASIZE)*2
	bfntheader = (RETSIZE+0)*2

	mov	CX,SS:[BX+handle]
	_les	BX,SS:[BX+bfntheader]
   l_	<mov	DX,ES:[BX].extSize>
   s_	<mov	DX,[BX].extSize>
;	mov	AX,4200h	; DOS: SEEK HANDLE (from TOP)
	mov	AX,4201h	; DOS: SEEK HANDLE (from current)
	mov	BX,CX
	xor	CX,CX
;	add	DX,BFNT_HEADER_SIZE
;	adc	CX,CX
	int	21h
	mov	AX,InvalidData
	jc	short QUIT
	mov	AX,NoError
QUIT:
	ret	(DATASIZE+1)*2
endfunc

END
