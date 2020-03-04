; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	_text_cursor_on
;	_text_cursor_off
;
; Parameters:
;	
;
; Returns:
;	
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
;	Kazumi(‰œ“c  m)
;	—ö’Ë(—ö’Ëº•F)
;
; Revision History:
;
;$Id: cursor.asm 0.02 92/05/29 19:59:00 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.MODEL SMALL
	include func.inc
	.CODE

func _TEXT_CURSOR_ON
	mov	AH,11h
	int	18h
	ret
endfunc

func _TEXT_CURSOR_OFF
	mov	AH,12h
	int	18h
	ret
endfunc

END
