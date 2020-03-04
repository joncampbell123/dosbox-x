; master library - (pf.lib)
;
; Description:
;	データ列の書き込み
;
; Functions/Procedures:
;	int bwrite(const void *buf,int size,bf_t bf);
;
; Parameters:
;	buf	書き込むデータのアドレス
;	size	バイト数
;	bf	bファイルハンドル
;
; Returns:
;	書き込んだバイト数
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
;	buf+sizeが10000hを越えてはならない。
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
; BWRITE.ASM         569 94-05-30    1:43
;	95/ 1/10 Initial: bwrite.asm/master.lib 0.23

	.model SMALL
	include func.inc

	.CODE
	EXTRN	BPUTC:CALLMODEL

func BWRITE	; bwrite() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	;arg	buf:dataptr,siz:word,bf:word
	buf	= (RETSIZE+3)*2
	siz	= (RETSIZE+2)*2
	bf	= (RETSIZE+1)*2

	CLD
	mov	SI,[BP+buf]
	mov	DI,[BP+siz]
	cmp	DI,0
	jle	short _return

_loop:
    s_ <lodsb>			; AH<-0は省略
    l_ <mov ES,[BP+buf+2]>
    l_ <lods byte ptr ES:[SI]>

	push	AX
	push	[BP+bf]
	_call	BPUTC

	inc	AH
	jz	short _return
	dec	DI
	jnz	short _loop
	inc	SI

_return:
	lea	AX,[SI-1]
	sub	AX,[BP+buf]

	pop	DI
	pop	SI
	pop	BP
	ret	(DATASIZE+2)*2
endfunc		; }

END
