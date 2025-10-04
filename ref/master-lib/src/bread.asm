; master library - (pf.lib)
;
; Description:
;	ファイルからの読み込み
;
; Functions/Procedures:
;	int bread(void *buf,int size,bf_t bf);
;
; Parameters:
;	buf	読み込み先
;	size	読み込みバイトバイト数
;	bf	bfileハンドル
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
;	buf+sizeが10000hを越えてはならない。
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
; BREAD.ASM         607 94-05-30   13:43
;	95/ 1/10 Initial: bread.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BGETC:CALLMODEL

func BREAD	; bread() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	;arg	buf:dataptr,siz:word,bf:word
	buf	= (RETSIZE+3)*2
	siz	= (RETSIZE+2)*2
	bf	= (RETSIZE+1)*2

	mov	DI,[BP+buf]
	mov	SI,[BP+siz]
	cmp	SI,0
	jle	short _return

_loop:	push	[BP+bf]
	_call	BGETC

	inc	AH
	jz	short _return
	s_mov	[DI],AL
	_mov	ES,[BP+buf+2]
	_mov	ES:[DI],AL
	inc	DI
	dec	SI
	jnz	short _loop

_return:
	mov	AX,DI
	sub	AX,[BP+buf]

	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc		; }

END
