; master library - (pf.lib)
;
; Description:
;	書き込み用に開いたファイルを閉じる
;
; Functions/Procedures:
;	void bclosew(bf_t bf);
;
; Parameters:
;	bf	bファイルハンドル
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
;	CPU: 186
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
;	iR
;	恋塚昭彦
;
; Revision History:
; BCLOSEW.ASM         279 94-05-26   12:57
;	95/ 1/10 Initial: bclosew.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BFLUSH:CALLMODEL
	EXTRN	BCLOSER:CALLMODEL

func BCLOSEW	; bclosew() {
	push	BP
	mov	BP,SP

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	push	[BP+bf]
	_call	BFLUSH
	push	[BP+bf]
	_call	BCLOSER

	pop	BP
	ret	(1)*2
endfunc		; }

END
