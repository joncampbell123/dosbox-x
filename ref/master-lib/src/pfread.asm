; master library - (pf.lib)
;
; Description:
;	データ列の読み込み
;
; Functions/Procedures:
;	unsigned pfread(void far *buf,unsigned size,pf_t pf);
;
; Parameters:
;	buf	読み込み先
;	size	読み込むバイト数
;	pf	pファイルハンドル
;
; Returns:
;	読み込んだバイト数
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
;	(bufの下位)+sizeが10000hを越えてはならない。
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
; PFREAD.ASM         679 94-09-17   23:19
;	95/ 1/10 Initial: pfread.asm.asm/master.lib 0.23
;	95/ 1/19 MODIFY: 内部1バイト読み込みルーチンをレジスタ渡しに変更

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func PFREAD	; pfread() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	;arg	buf:dword,siz:word,pf:word
	buf	= (RETSIZE+3)*2
	siz	= (RETSIZE+2)*2
	pf	= (RETSIZE+1)*2

	CLD
	mov	SI,[BP+siz]
	mov	DI,[BP+buf]
	test	SI,SI
	jz	short _return

_loop:
	mov	ES,[BP+pf]		; PFILE構造体のセグメント
	call	ES:[pf_getc]

	inc	AH
	jz	short _return
	mov	ES,[BP+buf + 2]
	stosb
	dec	SI
	jnz	short _loop

_return:
	mov	AX,DI
	sub	AX,[BP+buf]

	pop	DI
	pop	SI
	pop	BP
	ret	(4)*2
endfunc	; }

END
