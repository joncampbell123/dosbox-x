; superimpose & master library module
;
; Description:
;	BFNTファイルから消去パターンを一括設定
;
; Functions/Procedures:
;	int super_change_erase_bfnt( int patnum, const char * filename ) ;
;
; Parameters:
;	patnum    変更される最初の登録されているパターン番号
;	filename  BFNTファイル名
;
; Returns:
;	NoError       成功
;	FileNotFound  指定された BFNT ファイルがオープンできない
;	InvalidData   フォントファイルが使えない規格である
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	small data modelでは、DS != SSのときに使わないでください。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: supercgb.asm 0.03 93/01/15 11:46:10 Kazumi Rel $
; 
;3/10?
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/11/22 [M0.21] bugfix, return sizeが変だった。ありゃあ
;	95/ 3/25 [M0.22k] BUGFIX bfnt_change_erase_patエラー時に正常終了してた
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	FONTFILE_OPEN:CALLMODEL, BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_SKIP:CALLMODEL, BFNT_PALETTE_SKIP:CALLMODEL
	EXTRN	BFNT_CHANGE_ERASE_PAT:CALLMODEL, FONTFILE_CLOSE:CALLMODEL

func SUPER_CHANGE_ERASE_BFNT	; super_change_erase_bfnt() {
	push	BP
	mov	BP,SP
	sub	SP,BFNT_HEADER_SIZE

	patnum	= (RETSIZE+1+DATASIZE)*2
	filename= (RETSIZE+1)*2

	header	= -BFNT_HEADER_SIZE

	_push	[BP+filename+2]
	push	[BP+filename]
	_call	FONTFILE_OPEN
	jc	short OPEN_ERROR

	push	SI
	push	DI

	mov	SI,AX			;file handle
	lea	DI,[BP+header]
	EVEN
	push	SI	; handle
	_push	SS
	push	DI	; header
	_call	BFNT_HEADER_READ
	jc	short ERROR

	mov	AX,[BP+header].hdrSize
	or	AX,AX
	jz	short no_exthdr
	push	SI	; handle
	_push	DS
	push	DI	; header
	_call	BFNT_EXTEND_HEADER_SKIP
	EVEN
no_exthdr:
	test	[BP+header].col,80h	;palette table check
	jz	short palette_end
	push	SI	; handle
	_push	SS
	push	DI	; header
	_call	BFNT_PALETTE_SKIP
	EVEN
palette_end:
	push	[BP+patnum]	; patnum
	push	SI		; handle
	_push	SS
	push	DI		; header
	_call	BFNT_CHANGE_ERASE_PAT	; (patnum, handle, BFNT.. * header)
	jc	short ERROR

	push	SI		; handle
	_call	FONTFILE_CLOSE
	jmp	short RETURN

ERROR:
	push	AX
	push	SI		; handle
	_call	FONTFILE_CLOSE
	pop	AX
	stc

RETURN:
	pop	DI
	pop	SI
OPEN_ERROR:
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
endfunc		; }

END
