; master library - 
;
; Description:
;	G-GDCに、ワードパラメータを出力する
;
; Subroutines:
;	gdc_outpw
;
; Parameters:
;	AX: 出力するワードデータ
;
; Returns:
;	AL破壊
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: 8086
;	GDC
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
;	恋塚昭彦
;
; Revision History:
;	93/ 5/13 Initial:gdcoutpw.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE

GDC_PARAM   equ 0a0h

	public gdc_outpw
gdc_outpw proc near	; {
	out	GDC_PARAM,AL
	mov	AL,AH
	jmp	short $+2
	jmp	short $+2
	out	GDC_PARAM,AL
	ret
gdc_outpw endp		; }

END
