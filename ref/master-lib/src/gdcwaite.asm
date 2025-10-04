; master library - 
;
; Description:
;	G-GDCÇÃFIFOÇ™ãÛÇ…Ç»ÇÈÇ‹Ç≈ë“Ç¬
;
; Function/Procedures:
;	void gdc_waitempty( void ) ;
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
;	PC-9801
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
;	óˆíÀè∫ïF
;
; Revision History:
;	93/ 5/13 Initial:gdcwaite.asm/master.lib 0.16


	.MODEL SMALL
	include func.inc

	.CODE

GDC_STAT   equ 0a0h

func GDC_WAITEMPTY	; {
	jmp	short $+2
	in	AL,GDC_STAT
	test	AL,4
	jz	short GDC_WAITEMPTY
	ret
endfunc			; }

END
