; master library - DOS
;
; Description:
;	ベリファイフラグのON
;
; Functions/Procedures:
;	void dos_set_verify_on(void);
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
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;
; Revision History:
;	95/ 1/31 Initial: dossetvn.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.CODE
func	DOS_SET_VERIFY_ON	; dos_set_verify_on() {
	mov	AX,2e01h
	xor	DL,DL
	int	21h			; set verify flag
	ret
endfunc				; }

END
