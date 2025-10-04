; master library - (pf.lib)
;
; Description:
;	2バイト整数の書き出し
;
; Functions/Procedures:
;	int bputw(int w,bf_t bf);
;
; Parameters:
;	w	書き込む値
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
; BPUTW.ASM         421 94-05-26   21:47
;	95/ 1/10 Initial: bputw.asm/master.lib 0.23

	.model SMALL
	include func.inc

	.CODE
	EXTRN	BPUTC:CALLMODEL

func BPUTW	; bputw() {
	push	BP
	mov	BP,SP

	;arg	w:word,bf:word
	w	= (RETSIZE+2)*2
	bf	= (RETSIZE+1)*2

	mov	AL,byte ptr [BP+w]
	push	AX
	push	[BP+bf]
	_call	BPUTC

	test	AX,AX
	js	short _return

	mov	AL,byte ptr [BP+w+1]
	push	AX
	push	[BP+bf]
	_call	BPUTC

	test	AX,AX
	js	short _return
	mov	AX,[BP+w]

_return:
	pop	BP
	ret	2*2
endfunc		; }

END
