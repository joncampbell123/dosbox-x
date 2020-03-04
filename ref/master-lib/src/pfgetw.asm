; master library - (pf.lib)
;
; Description:
;	2バイト整数の読み込み
;
; Functions/Procedures:
;	int pfgetw(pf_t pf)
;
; Parameters:
;	pf	pfファイルハンドル
;
; Returns:
;	
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
; PFGETW.ASM         345 94-09-17   22:53
;	95/ 1/10 Initial: pfgetw.asm/master.lib 0.23
;	95/ 1/19 MODIFY: 内部1バイト読み込みルーチンをレジスタ渡しに変更

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func PFGETW	; pfgetw() {
	push	BP
	mov	BP,SP

	;arg	pf:word
	pf	= (RETSIZE+1)*2

	mov	ES,[BP+pf]
	call	ES:[pf_getc]

	push	AX
	call	ES:[pf_getc]
	mov	AH,AL
	pop	BX
	mov	AL,BL

	pop	BP
	ret	(1)*2
endfunc	; }

END
