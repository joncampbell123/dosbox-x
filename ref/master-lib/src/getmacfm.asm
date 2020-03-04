; Hiper library
;
; Description:
;	FM-R ‚©‚Ç‚¤‚©‚ð check ‚·‚é
;
; Procedures/Functions:
;	int check_machine_fmr( void );
;
; Parameters:
;
; Returns:
;	==0  : FM-R
;	!=0 : not FM-R
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	‚Ì‚ë/V(H.Maeda)
;
; Revision History:
;	94/ 3/25 Initial

	.MODEL SMALL

	include func.inc

	.CODE

func CHECK_MACHINE_FMR
	mov	AX,0ffffh
	mov	ES,AX
	mov	AX,0fc00h
	sub	AX,ES:[0003h]
	ret
endfunc

END
