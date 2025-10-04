; superimpose & master library module
;
; Description:
;	�t�@�C���̓ǂݍ��ݗp�I�[�v��: Pascal�p���W���[��
;
; Functions/Procedures:
;	function fontfile_open( filename:String ) : Integer ;
;	function dos_ropen( filename:String ) : Integer ;	(�ʖ�)
;
; Parameters:
;	
;
; Returns:
;	integer	FileNotFound (cy=1)	���s,�J���Ȃ�
;		else	     (cy=0)	����,�t�@�C���n���h��
;
; Binding Target:
;	Turbo Pascal
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
;	93/ 5/ 9 Initial: fontopnp.asm/master.lib 0.16
;	94/ 6/16 [M0.23] file_sharingmode�ϐ��̒ǉ�

	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	STR_PASTOC:CALLMODEL

	.DATA
	EXTRN	file_sharingmode:WORD
	.CODE

func DOS_ROPEN		; ���x���t��
endfunc
func FONTFILE_OPEN	; {
	CLD
	push	BP
	mov	BP,SP
	sub	SP,256
	filename = (RETSIZE+1)*2
	cname	= -256

	_push	SS
	lea	AX,[BP+cname]
	push	AX
	_push	[BP+filename+2]
	push	[BP+filename]
	call	STR_PASTOC

	push	DS
	mov	AH,3dh
	mov	AL,byte ptr file_sharingmode
	push	SS
	pop	DS
	lea	DX,[BP+cname]
	int	21h			;handle open
	pop	DS
	mov	SP,BP
	pop	BP
	jc	short OPEN_ERROR
	ret	DATASIZE*2
	EVEN

OPEN_ERROR:
	mov	AX,FileNotFound
	ret	DATASIZE*2
endfunc			; }

END
