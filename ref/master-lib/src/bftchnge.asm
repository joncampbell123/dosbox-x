; superimpose & master library module
;
; Description:
;	BFNT+ファイルによってパターンの消去パターンを変更する
;
; Functions/Procedures:
;	int bfnt_change_erase_pat( int patnum, int handle, BFNT_HEADER * header ) ;
;
; Parameters:
;	patnum		先頭パターン番号
;	handle		読み込むBFNT+ファイルのハンドル
;	header		読み込むBFNT+ファイルのヘッダ構造体
;
; Returns:
;	NoError		(cy=0)  成功
;	InvalidData	(cy=1)  2色じゃない、または途中でファイル終端に達した
;	InsufficientMemory (cy=1) 一時的に作業するメモリが足りない
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
;$Id: bftchnge.asm 0.03 93/01/15 11:35:36 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	94/ 1/15 [M0.22] bugfix(x_x)

	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	SUPER_CHANGE_ERASE_PAT:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

	.CODE
func BFNT_CHANGE_ERASE_PAT	; bfnt_change_erase_pat() {
	push	BP
	mov	BP,SP
	sub	SP,4
	push	SI
	push	DI

	; 引数
	patnum = (RETSIZE+2+DATASIZE)*2
	handle = (RETSIZE+1+DATASIZE)*2
	header = (RETSIZE+1)*2

	plane_size = -2
	max_pat    = -4

	_push	DS
	_lds	BX,[BP+header]
	mov	AX,[BX].Xdots
	mov	CL,3
	shr	AX,CL
	mul	[BX].Ydots
	mov	[BP+plane_size],AX
	mov	CX,[BX].END_
	sub	CX,[BX].START
	cmp	[BX].col,0		; 2色?
	_pop	DS
	mov	AX,InvalidData
	stc
	jne	short RETURN

	push	[BP+plane_size]
	_call	SMEM_WGET
	jc	short RETURN

	mov	DI,AX
	mov	SI,[BP+patnum]
	add	CX,SI
	mov	[BP+max_pat],CX

NEXT:	push	DS
	mov	DS,DI
	xor	DX,DX
	mov	CX,[BP+plane_size]
	mov	BX,[BP+handle]
	mov	AH,3fh
	int	21h		; read handle
	pop	DS
	mov	AX,InvalidData
	jc	short FAILURE

	push	SI		; patnum
	push	DI		; segment
	xor	AX,AX
	push	AX		; offset
	_call	SUPER_CHANGE_ERASE_PAT

	inc	SI
	cmp	SI,[BP+max_pat]
	jle	short NEXT

	xor	AX,AX			; NoError

FAILURE:
	push	DI
	_call	SMEM_RELEASE
RETURN:
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	(2+DATASIZE)*2
endfunc				; }

END
