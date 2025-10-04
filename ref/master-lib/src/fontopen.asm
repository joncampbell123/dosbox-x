; superimpose & master library module
;
; Description:
;	�t�@�C���̓ǂݍ��ݗp�I�[�v��: C����p���W���[��
;
; Functions/Procedures:
;	fontfile_open
;	dos_ropen		(�ʖ�)
;
; Parameters:
;	
;
; Returns:
;	int	FileNotFound (cy=1)	���s,�J���Ȃ�
;		else	     (cy=0)	����,�t�@�C���n���h��
;
; Binding Target:
;	Microsoft-C / Turbo-C
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
;
;$Id: fontopen.asm 0.04 93/01/15 11:41:40 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	94/ 6/16 [M0.23] file_sharingmode�ϐ��̒ǉ�
;

	.MODEL SMALL
	include func.inc
	include	super.inc

	.DATA
	public file_sharingmode,_file_sharingmode
_file_sharingmode	label word
file_sharingmode	dw	0

	.CODE

func DOS_ROPEN		; ���x���t��
endfunc
func FONTFILE_OPEN	; {
	mov	BX,SP
	filename = (RETSIZE+0)*2

	mov	AH,3dh
	mov	AL,byte ptr file_sharingmode
	_push	DS
	_lds	DX,SS:[BX+filename]
	int	21h			;handle open
	_pop	DS
	jc	short OPEN_ERROR
	ret	DATASIZE*2
	EVEN

OPEN_ERROR:
	mov	AX,FileNotFound
	ret	DATASIZE*2
endfunc			; }

END
