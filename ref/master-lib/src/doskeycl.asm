; master library - MS-DOS
;
; Description:
;	DOS�̃L�[�o�b�t�@�̃N���A
;
; Function/Procedures:
;	void dos_keyclear( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	none
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
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_KEYCLEAR
	mov	AX,0c00h
	int	21h
	ret
endfunc

END
