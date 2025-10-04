; master library - DOS
;
; Description:
;	�x���t�@�C�t���O�̎擾
;
; Functions/Procedures:
;	int dos_get_verify(void);
;
; Parameters:
;	none
;
; Returns:
;	0 = verify on
;	0�ȊO = verify off
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
;	Kazumi(���c  �m)
;
; Revision History:
;	95/ 1/31 Initial: dosgetve.asm/master.lib 0.23

	.MODEL SMALL

	include func.inc

	.CODE
func	DOS_GET_VERIFY	; dos_get_verify() {
	mov	AH,54h
	int	21h			; get verify flag
	xor	AH,AH
	ret
endfunc			; }

END
