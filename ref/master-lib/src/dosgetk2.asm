; master library - MS-DOS
;
; Description:
;	DOSE•W€“ü—Í‚©‚ç‚Ì•¶š“Ç‚İæ‚è‚R
;	(^C‚Å~‚Ü‚ç‚È‚¢, “ü—Í‚ª‚È‚­‚Ä‚à‘Ò‚½‚È‚¢,“ü—Í‚È‚µ‚ÆCTRL+@‚ª‹æ•Ê‚Å‚«‚é)
;
; Function/Procedures:
;	int dos_getkey2( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 .. 255 “ü—Í‚³‚ê‚½•¶š
;		-1 ...... “ü—Í‚È‚µ
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
;	92/12/18 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETKEY2
	mov	AH,6
	mov	DL,0FFh
	int	21h
	mov	AH,0
	jnz	short OK
	dec	AX
OK:
	ret
endfunc

END
