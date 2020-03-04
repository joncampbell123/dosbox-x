; master library - MSDOS
;
; Description:
;	メインメモリブロックの開放
;
; Function/Procedures:
;	void mem_free( unsigned seg ) ;
;	void dos_free( unsigned seg ) ;		※この二つは同じものです
;
; Parameters:
;	unsigned seg	DOSメモリブロックのセグメント
;
; Returns:
;	None
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 2.0 or later
;
; Notes:
;	AX以外の全レジスタを保存します
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 1/31 Initial dosfree.asm(from memalloc.asm)
;	93/ 3/29 があんbugfix

	.MODEL SMALL
	include func.inc
	.CODE

	public DOS_FREE
func MEM_FREE
DOS_FREE label CALLMODEL
	push	BP
	push	ES
	mov	BP,SP
	mov	ES,[BP+(RETSIZE+2)*2]	; seg
	mov	AH,49h
	int	21h
	pop	ES
	pop	BP
	ret	2
endfunc

END
