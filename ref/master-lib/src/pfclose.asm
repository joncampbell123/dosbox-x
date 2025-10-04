; master library - (pf.lib)
;
; Description:
;	ファイルを閉じ、ハンドルを開放する
;
; Functions/Procedures:
;	void pfclose(pf_t pf);
;
; Parameters:
;	pf	pファイルハンドル
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
; PFCLOSE.ASM         318 94-09-17   23:02
;	95/ 1/10 Initial: pfclose.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BCLOSER:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

func PFCLOSE	; pfclose() {
	push	BP
	mov	BP,SP

	;arg	pf:word
	pf	= (RETSIZE+1)*2

	mov	ES,[BP+pf]		; PFILE構造体のセグメント
	push	ES:[pf_bf]
	_call	BCLOSER

	push	[BP+pf]
	_call	HMEM_FREE

	pop	BP
	ret	(1)*2
endfunc		; }

END
