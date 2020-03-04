; master library - (pf.lib)
;
; Description:
;	DOSファイルハンドルを指定して bファイルハンドルを作成する
;
; Functions/Procedures:
;	bf_t bdopen(int handle);
;
; Parameters:
;	handle	DOSファイルハンドル
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
; BDOPEN.ASM         569 94-06-05   18:44
;	95/ 1/10 Initial: bdopen.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.model SMALL
	include func.inc
	include pf.inc
	include super.inc

	.DATA
	EXTRN	pferrno:BYTE
	EXTRN	bbufsiz:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL

func BDOPEN	; bdopen() {
	push	BP
	mov	BP,SP

	;arg	handle:word
	handle	= (RETSIZE+1)*2

	mov	mem_AllocID,MEMID_bfile
	mov	AX,bbufsiz
	add	AX,size BFILE
	push	AX
	_call	HMEM_ALLOCBYTE
	mov	pferrno,PFENOMEM
	jc	short _return

	mov	ES,AX			; BFILE構造体のセグメント
	mov	CX,[BP+handle]
	mov	ES:[b_hdl],CX
	mov	ES:[b_pos],0		; bdopenw()用
	mov	ES:[b_left],0		; bdopenr()用
	mov	CX,bbufsiz
	mov	ES:[b_siz],CX

_return:
	pop	BP
	ret	(1)*2
endfunc		; }

END
