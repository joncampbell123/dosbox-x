; master library - (pf.lib)
;
; Description:
;	書き込み用にファイルを開く
;
; Functions/Procedures:
;	bf_t bopenw(const char *fname);
;
; Parameters:
;	fname	ファイル名
;
; Returns:
;	0	エラー。pferrnoに種別が入る。
;		PFENOTOPEN	ファイルが開かない
;		PFENOMEM	メモリ不足
;
;	0以外	b*系ファイル関数に渡すハンドル
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
; BOPENW.ASM         736 94-05-30   22:10
;	95/ 1/10 Initial: bopenw.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.186
	.model SMALL
	include func.inc
	include pf.inc
	include super.inc

	.DATA
	EXTRN	pferrno:BYTE
	EXTRN	bbufsiz:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	DOS_CREATE:CALLMODEL
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

func BOPENW	; bopenw() {
	push	BP
	mov	BP,SP

	;arg	fname:dataptr
	fname	= (RETSIZE+1)*2

	mov	mem_AllocID,MEMID_bfile
	mov	AX,bbufsiz
	add	AX,size BFILE
	push	AX
	_call	HMEM_ALLOCBYTE
	jc	short _nomem

	mov	ES,AX			; BFILE構造体のセグメント

	_push	[BP+fname+2]
	push	[BP+fname]
	push	0			; ファイル属性(normal)
	_call	DOS_CREATE
	jc	short _cantopen

	mov	ES:[b_hdl],AX
	mov	ES:[b_pos],0
	mov	AX,bbufsiz
	mov	ES:[b_siz],AX

	mov	AX,ES
	pop	BP
	ret	(DATASIZE)*2

_nomem:
	mov	pferrno,PFENOMEM
	jmp	short _error
_cantopen:
	push	ES
	_call	HMEM_FREE
	mov	pferrno,PFENOTOPEN
_error:
	clr	AX
	pop	BP
	ret	(DATASIZE)*2
endfunc		; }

END
