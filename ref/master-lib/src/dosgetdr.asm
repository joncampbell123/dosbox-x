; master library - MS-DOS
;
; Description:
;	�J�����g�h���C�u�̓ǂݎ��
;
; Function/Procedures:
;	int dos_getdrive( void ) ;
;
; Parameters:
;	char * path	�f�B���N�g��
;
; Returns:
;	�h���C�u; A:=0, B:=1...
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

func DOS_GETDRIVE
	mov	AH,19h
	int	21h
	xor	AH,AH
	ret
	EVEN
endfunc

END
