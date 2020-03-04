; master library - (pf.lib)
;
; Description:
;	1バイト書き出し
;
; Functions/Procedures:
;	int bputc(int c,bf_t bf);
;
; Parameters:
;	c	書き込むバイト
;	bf	bファイルハンドル
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
; BPUTC.ASM         509 94-06-05   17:57
;	95/ 1/10 Initial: bputc.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BFLUSH:CALLMODEL

func BPUTC	; bputc() {
	push	BP
	mov	BP,SP

	;arg	c:word,bf:word
	c	= (RETSIZE+2)*2
	bf	= (RETSIZE+1)*2

	mov	ES,[BP+bf]		; BFILE構造体のセグメント

	mov	BX,ES:[b_pos]
	mov	AX,[BP+c]
	mov	ES:[b_buff][BX],AL

	inc	BX
	mov	ES:[b_pos],BX
	cmp	BX,ES:[b_siz]
	jb	short _ok

	push	ES
	_call	BFLUSH
	chk	AX
	jnz	short _return

	mov	AX,[BP+c]

_ok:
	clr	AH

_return:
	pop	BP
	ret	(2)*2
endfunc		; }

END
