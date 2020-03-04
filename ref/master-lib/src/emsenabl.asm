; master library - PC98 - MSDOS - EMS
;
; Description:
;	NEC EMSドライバのページフレームの用途の制御
;
; Function/Procedures:
;	void ems_enablepageframe( int enable ) ;
;
; Parameters:
;	int enable	0=disable(B000h=VRAM), 1=enable(B000=EMS)
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
;	EMS: LIM EMS 4.0
;
; Notes:
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
;	92/11/16 Initial


	.MODEL SMALL
	include func.inc
	.CODE

func	EMS_ENABLEPAGEFRAME
	mov	BX,BP
	mov	BX,SS:[BX+(RETSIZE)*2]
	mov	AX,7001h
	int	67h
	ret	2
endfunc

END
