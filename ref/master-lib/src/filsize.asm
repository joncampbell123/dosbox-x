; master library - DOS
;
; Description:
;	�t�@�C���̃T�C�Y�𓾂�
;
; Function/Procedures:
;	long file_size( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	-1   �J����Ă��Ȃ�
;	else ���ݓǂݍ��݃I�[�v������Ă���t�@�C���̃T�C�Y(�o�C�g��)
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
;	���ˏ��F
;
; Revision History:
;	92/12/11 Initial
;	94/ 2/14 [M0.22a] dos_filesize�Ăяo���ɕύX

	.MODEL SMALL
	include func.inc
	EXTRN DOS_FILESIZE:CALLMODEL

	.DATA
	EXTRN file_Handle:WORD

	.CODE
func FILE_SIZE	; file_size() {
	push	file_Handle
	_call	DOS_FILESIZE
	jc	short FAULT
	ret
FAULT:
	mov	AX,DX	; -1�ɂ���
	ret
endfunc		; }

END
