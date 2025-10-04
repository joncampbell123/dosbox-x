; master library - (pf.lib)
;
; Description:
;	ファイルポインタの相対移動
;
; Functions/Procedures:
;	unsigned long pfseek(pf_t pf,unsigned long offset)
;
; Parameters:
;	pf	pファイルハンドル
;	offset	移動量
;
; Returns:
;	現在のファイルポインタ
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
; PFSEEK.ASM         513 94-09-17   23:07
;	95/ 1/10 Initial: pfseek.asm/master.lib 0.23
;	95/ 1/19 MODIFY: 内部1バイト読み込みルーチンをレジスタ渡しに変更
;	95/ 1/21 MODIFY: 戻り値をvoid->unsigned longに(tell機能)

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func PFSEEK	; pfseek() {
	push	BP
	mov	BP,SP
	push	DI

	;arg	pf:word,loc:dword
	pf	= (RETSIZE+3)*2
	loc	= (RETSIZE+1)*2

	mov	ES,[BP+pf]		; PFILE構造体のセグメント

	inc	word ptr [BP+loc + 2]
	mov	DI,word ptr [BP+loc]
	or	DI,DI
	jz	short _next
_loop:
	call	ES:[pf_getc]
	test	AH,AH
	jnz	short _over

	dec	DI
	jnz	short _loop
_next:	dec	word ptr [BP+loc + 2]
	jnz	short _loop
_over:
	mov	AX,word ptr ES:[pf_loc]
	mov	DX,word ptr ES:[pf_loc+2]

	pop	DI
	pop	BP
	ret	(3)*2
endfunc	; }

END
