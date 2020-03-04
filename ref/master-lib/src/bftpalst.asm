; superimpose & master library module
;
; Description:
;	BFNTファイルからパレット情報を読み取り、Palettesに設定する
;
; Functions/Procedures:
;	int bfnt_palette_set( int handle, BfntHeader * header ) ;
;
; Parameters:
;	handle	BFNT+ファイルのファイルハンドル
;	header	BFNT+ファイルのヘッダ
;
; Returns:
;	NoError		正常
;	InvalidData	パレット情報が存在しない
;	InvalidData	ファイルポインタの移動ができない
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
;	ハードウェアパレットへの設定は行いません。
;	必要に応じて、palette_show(), black_in()などを呼び出して下さい。
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
;$Id: bftpalst.asm 0.05 93/02/19 20:07:23 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
	include	super.inc

	.DATA
	EXTRN Palettes : BYTE

	.CODE

func BFNT_PALETTE_SET
	push	BP
	mov	BP,SP
	handle	= (RETSIZE+1+DATASIZE)*2
	header	= (RETSIZE+1)*2

	_push	DS
	_lds	BX,[BP+header]
	test	[BX].col,80h
	_pop	DS
	jz	short INVALID

file_ok:
	mov	AH,3fh
	mov	BX,[BP+handle]
	mov	CX,48			;16色 * 3
	mov	DX,offset Palettes
	int	21h			;read handle
	jc	short INVALID

	; パレットデータの変換
	mov	BX,DX
	mov	CX,1004h
PALLOOP:
	mov	DL,[BX]		; b
	mov	AX,[BX+1]	; r,g
	mov	[BX],AX		; r,g
	mov	[BX+2],DL	; b
	add	BX,3
	dec	CH
	jnz	short PALLOOP

	mov	AX,NoError
	jmp	short OWARI
INVALID:
	stc
	mov	AX,InvalidData
OWARI:
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
