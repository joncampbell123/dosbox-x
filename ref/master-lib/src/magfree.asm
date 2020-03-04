; master library - 
;
; Description:
;	magイメージの開放
;
; Function/Procedures:
;	void mag_free( MagHeader * magheader, void far * image ) ;
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
;	8086
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
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 9 Initial: magfree.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc
	EXTRN HMEM_FREE:CALLMODEL

	.CODE
func MAG_FREE	; mag_free() {
	push	BP
	mov	BP,SP
	; 引数
	magheader = (RETSIZE+3)*2
	image	  = (RETSIZE+1)*2
	mov	CX,[BP+image+2]
	jcxz	short NO_IMAGE
	push	CX
	_call	HMEM_FREE
NO_IMAGE:

	s_push	DS
	s_pop	ES
	_les	BX,[BP+magheader]
	mov	CX,ES:[BX]
	jcxz	short NO_COMMENT
	push	CX
	_call	HMEM_FREE
	xor	CX,CX
	mov	ES:[BX],CX
	mov	ES:[BX+2],CX
NO_COMMENT:
	pop	BP
	ret	(2+DATASIZE)*2
endfunc		; }

END
