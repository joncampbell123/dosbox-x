; master library - MS-DOS
;
; Description:
;	DOSE•W€“ü—Í‚©‚ç‚Ì•¶š“Ç‚İæ‚è
;	(^C‚Å~‚Ü‚ç‚È‚¢)
;
; Function/Procedures:
;	int dos_getch( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	“ü—Í‚³‚ê‚½•¶š
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

func DOS_GETCH
	mov	AH,7
	int	21h
	xor	AH,AH
	ret
endfunc

END
