; master library - at - font
;
; Description:
;	PC/AT, メモリ半角フォントへのBIOSからの登録
;
; Functions/Procedures:
;	int font_at_entry_cgrom(int firstchar, int lastchar) ;
;
; Parameters:
;	文字コード
;
; Returns:
;	NoError			(cy=0) 成功
;	InsufficientMemory	(cy=1) メモリ不足
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
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
;	恋塚昭彦
;
; Revision History:
;	94/ 8/ 4 Initial: fontatcg.asm/master.lib 0.23
;	95/ 3/30 [M0.22k] メモリ不足対応

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

	EXTRN	FONT_WRITE:CALLMODEL	; fontwrit.asm

	.DATA
	EXTRN	font_ReadEndChar:WORD	; fontcg.asm
	EXTRN	font_ReadChar:WORD	; fontcg.asm

	.CODE
	EXTRN	FONT_AT_READ:CALLMODEL	; fontatre.asm

func FONT_AT_ENTRY_CGROM	; font_at_entry_cgrom() {
	enter	32,0
	push	SI
	push	DI

	; 引数
	firstchar = (RETSIZE+2)*2
	lastchar = (RETSIZE+1)*2

	buf	= -32

	mov	SI,[BP+firstchar]
	mov	DI,[BP+lastchar]
	and	SI,0ffh
	and	DI,0ffh

CGROM_LOOP:
	push	SI
	push	0810h
	push	SS
	lea	AX,[BP+buf]
	push	AX
	_call	FONT_AT_READ
	push	SI
	_push	SS
	lea	AX,[BP+buf]
	push	AX
	call	FONT_WRITE
	mov	AX,InsufficientMemory
	jc	short DONE
	inc	SI
	cmp	SI,DI
	jle	short CGROM_LOOP

	mov	font_ReadEndChar,DI
	inc	DI
	mov	font_ReadChar,DI

	xor	AX,AX			; NoError
DONE:
	pop	DI
	pop	SI
	leave
	ret	4
endfunc		; }

END
