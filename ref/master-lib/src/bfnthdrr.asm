; superimpose & master library module
;
; Description:
;	BFNTファイルのヘッダを読み取る
;
; Functions/Procedures:
;	int bfnt_header_read( unsigned handle , void * bfntheader) ;
;
; Parameters:
;	handle		ファイルハンドル
;	bfntheader	読み込み先
;
; Returns:
;	NoError		(cy=0)	読み取り成功
;	InvalidData	(cy=1)	読み取り失敗
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
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
;$Id: bfnthdrr.asm 0.03 93/01/15 11:33:46 Kazumi Rel $
;
;	93/ 3/17 Initial: master.lib <- super.lib 0.22b
;	94/ 6/10 [M0.23] エラー時、DOSのエラーならsuper error codeに変換する
;

	.MODEL SMALL
	include func.inc
	include	super.inc

	.DATA
	EXTRN BFNT_ID:BYTE

	.CODE

func BFNT_HEADER_READ	; {
	push	BP
	mov	BP,SP

	; 引数
	handle	   = (RETSIZE+1+DATASIZE)*2
	bfntheader = (RETSIZE+1)*2

	_push	DS
	_lds	DX,[BP+bfntheader]
	mov	BX,[BP+handle]

	mov	AH,3fh			; DOS: READ HANDLE
	mov	CX,BFNT_HEADER_SIZE
	int	21h

	push	DS
	pop	ES
	_pop	DS

	sbb	CX,CX
	xor	AX,CX
	sub	AX,CX	; エラーなら符号反転,cy=1
	jc	short ILLEGAL

	push	SI
	push	DI
	mov	SI,offset BFNT_ID
	mov	DI,DX
	mov	CX,5
	repe	cmpsb
	pop	DI
	pop	SI

	jne	short INVALID
	mov	AX,NoError
	pop	BP
	ret	(DATASIZE+1)*2

INVALID:
	mov	AX,InvalidData
	stc
ILLEGAL:
	pop	BP
	ret	(DATASIZE+1)*2
endfunc			; }

END
