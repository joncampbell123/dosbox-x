; master library - PC98 - MSDOS - EMS
;
; Description:
;	EMSメモリハンドルに名前をつける
;
; Function/Procedures:
;	int ems_setname( unsigned handle, const char * hname );
;
; Parameters:
;	unsigned handle EMSハンドル
;	char * hname	8bytes,余っている場合は'\0'で埋めること
;
; Returns:
;	0 ........... success
;	80h～89h .... failure(EMS エラーコード)
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
;	92/11/18 bugfix ( thanks, Mikio! )

	.MODEL SMALL
	include func.inc

	.CODE

func	EMS_SETNAME
	push	SI
	mov	SI,SP
	_push	DS

	; 引数
	handle	= (RETSIZE+1+DATASIZE)*2
	hname	= (RETSIZE+1)*2

	mov	DX,SS:[SI+handle]
	_lds	SI,SS:[SI+hname]

	mov	AX,5301h	; function: Set Handle Name
	int	67h		; call EMM
	mov	AL,AH
	xor	AH,AH

	_pop	DS
	pop	SI
	ret	(1+DATASIZE)*2
endfunc

END
