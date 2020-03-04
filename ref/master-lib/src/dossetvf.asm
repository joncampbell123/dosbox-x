; master library - DOS
;
; Description:
;	ベリファイフラグのOFF
;
; Functions/Procedures:
;	void dos_set_verify_off(void);
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
;	95/ 1/31 Initial: dossetvf.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.CODE
func	DOS_SET_VERIFY_OFF
	mov	AX,2e00h
	xor	DL,DL
	int	21h			; set verify flag
	ret
endfunc

END
