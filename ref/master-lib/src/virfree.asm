; superimpose & master library module
;
; Description:
;	âºëzVRAMÇÃäJï˙
;
; Function/Procedures:
;	void virtual_free(void) ;
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
;	
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
;	óˆíÀè∫ïF
;
; Revision History:
;	93/ 9/16 Initial: virfree.asm/master.lib 0.21
;	95/ 4/ 1 [M0.22k] virtual_vramwordsëŒâû

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN virtual_seg:WORD		; virtual.asm
	EXTRN virtual_vramwords:WORD	; virtual.asm

	.CODE
	EXTRN	HMEM_FREE:CALLMODEL

func VIRTUAL_FREE	; virtual_free() {
	mov	virtual_vramwords,0
	mov	AX,virtual_seg
	test	AX,AX
	jz	short OK
	push	AX
	call	HMEM_FREE
	mov	virtual_seg,0
OK:
	ret
endfunc		; }

END
