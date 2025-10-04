; master library - MS-DOS
;
; Description:
;	•W€o—Í‚É•¶š‚ğo—Í‚·‚é(^CŒŸ¸‚È‚µ)
;
; Function/Procedures:
;	void dos_putc( int c ) ;
;
; Parameters:
;	int c		•¶š
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
;	—ö’Ëº•F
;
; Revision History:
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_PUTC
	mov	BX,SP
	; ˆø”
	c	= RETSIZE * 2

	mov	DL,SS:[BX+c]
	mov	AH,6
	int	21h
	ret	2
	EVEN
endfunc

END
