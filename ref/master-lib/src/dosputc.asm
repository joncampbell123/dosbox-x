; master library - MS-DOS
;
; Description:
;	�W���o�͂ɕ������o�͂���(^C�����Ȃ�)
;
; Function/Procedures:
;	void dos_putc( int c ) ;
;
; Parameters:
;	int c		����
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

func DOS_PUTC
	mov	BX,SP
	; ����
	c	= RETSIZE * 2

	mov	DL,SS:[BX+c]
	mov	AH,6
	int	21h
	ret	2
	EVEN
endfunc

END
