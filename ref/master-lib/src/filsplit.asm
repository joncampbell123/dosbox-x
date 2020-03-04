; master library - 
;
; Description:
;	path を4つの構成要素に分解する
;
; Functions/Procedures:
;	void file_splitpath(const char *path, char *drv, char *dir, char *base, char *ext)
;
; Parameters:
;	path	分解したい元パス名( e.g. "A:\HOGE\ABC.C" )	NULL禁止
;	drv	ドライブ部分の格納先( "A:" )			NULL可
;	dir	パス部分の格納先( "\HOGE\" )			NULL可
;	base	ファイル名部分の格納先("ABC")			NULL可
;	ext	拡張子部分の格納先(".C")			NULL可
;
; Returns:
;	none
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
;	・drvは 3byte用意してあれば安全です
;	・dirは pathを丸ごとコピーできるだけの領域が必要です
;	・baseは、9byte用意してあれば安全です
;	・extは 6byte用意してあれば安全です
;
;	・drv,dir,base,extはNULLにすれば書き込みません。
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
;	94/ 5/30 Initial: filsplit.asm/master.lib 0.23
;	95/ 3/10 [M0.22k] BUGFIX: LARGE drv,dirがNULLのとき狂ってた
;	95/ 4/12 [M0.22k] BUGFIX: LARGE extが正しく格納されなかった

	.MODEL SMALL
	include func.inc
	.code

func FILE_SPLITPATH		; file_splitpath() {
	push	BP
	mov	BP,SP
	sub	SP,2
	push	SI
	push	DI

	;
	path = (RETSIZE+1+4*DATASIZE)*2
	drv  = (RETSIZE+1+3*DATASIZE)*2
	dir  = (RETSIZE+1+2*DATASIZE)*2
	base = (RETSIZE+1+1*DATASIZE)*2
	ext  = (RETSIZE+1+0*DATASIZE)*2

	;
	idir = -2

	xor	AX,AX
	mov	[BP+idir],AX
	xor	DI,DI		; ibase
	xor	DX,DX		; iext
	xor	CX,CX		; kflag
	xor	BX,BX		; i
	_push	DS
	_lds	SI,[BP+path]

NEXT:
	mov	AL,[BX+SI]
	cmp	AL,0
	je	short BREAK

SLOOP:
	inc	BX

	dec	CX
	jz	short NEXT

	cmp	AL,81h
	jl	short ISASCII
	cmp	AL,9fh
	jle	short ISKANJI
	cmp	AL,0e0h
	jl	short ISASCII
	cmp	AL,0fch
	jle	short ISKANJI
ISASCII:
	cmp	AL,':'
	je	short COLON
	cmp	AL,'\'
	je	short SLASH
	cmp	AL,'/'
	je	short SLASH
	cmp	AL,'.'
	je	short DOT

	mov	AL,[BX+SI]
	cmp	AL,0
	jne	short SLOOP
	jmp	short BREAK

ISKANJI:
	mov	CX,1
	jmp	short NEXT

COLON:	cmp	BX,2
	jne	short NEXT		; ':' 先頭から2文字目のときのみ
	mov	[BP+idir],BX		;     idirにセットし、'/'処理へ。
SLASH:	mov	DI,BX			; '/' DI(ファイル名先頭位置)セット,
	xor	DX,DX			;     DX(拡張子位置)消去。
	jmp	short NEXT

DOT:	mov	DX,BX			; '.' 拡張子位置セット
	dec	DX
	jmp	short NEXT

	; 文字列の走査が完了した。
	;  SI:   文字列の先頭アドレス
	;  idir: ドライブ名があるときはその長さ(2)が入っている,なければ0。
	;  DI:   ファイル名先頭の位置が入っている
	;  DX:   拡張子('.')の位置が入っている。なければ0。
	; 位置は文字列先頭からの距離。
BREAK:
	test	DX,DX
	jz	short NO_DOT	; '.'がないときは DXに文字列末尾位置をいれる
	mov	AX,SI
	add	SI,DI		; path+ibase
	cmp	byte ptr [SI],'.'
	mov	SI,AX
	jne	short L2	; '.'がファイル名先頭になければskip
	mov	CX,BX
	dec	CX
	cmp	CX,DX
	jne	short L2	; '.'がファイル名先頭と文字列末尾にあるなら、
IF 0	; 以下を無効にする。すなわち名前の両端が'.'ならファイル名扱いにする。
	mov	DI,BX		;    ファイル名先頭位置を末尾(=なし)に。
ENDIF
NO_DOT:	mov	DX,BX
L2:

	CLD
	s_push	DS
	s_pop	ES

	mov	AX,DI			; ibase

	mov	CX,[BP+idir]
	_les	DI,[BP+drv]
    l_ <or	[BP+drv+2],DI>
    s_ <test	DI,DI>
	jz	short NO_DRV
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_DRV:
	add	SI,CX

	mov	CX,AX
	sub	CX,[BP+idir]
	_les	DI,[BP+dir]
    l_ <or	[BP+dir+2],DI>
    s_ <test	DI,DI>
	jz	short NO_DIR
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_DIR:
	add	SI,CX

	_les	DI,[BP+base]
    l_ <or	[BP+base+2],DI>
    s_ <test	DI,DI>
	jz	short NO_BASE
	mov	CX,DX
	sub	CX,AX
	cmp	CX,8
	jle	short BASE8
	mov	CX,8
BASE8:	add	AX,CX
	rep 	movsb
	mov	byte ptr ES:[DI],0
NO_BASE:
	add	SI,DX
	sub	SI,AX

	_les	DI,[BP+ext]
	_mov	AX,ES
    l_ <or	AX,DI>
    s_ <test	DI,DI>
	jz	short NO_EXT
	movsw
	movsw
	mov	byte ptr ES:[DI],0
NO_EXT:

	_pop	DS
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	DATASIZE*2*5
endfunc		; }

END
