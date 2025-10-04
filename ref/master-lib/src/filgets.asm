; master library - MS-DOS
;
; Description:
;	ファイルからの文字列の読み込み
;
; Function/Procedures:
;	unsigned file_gets( char far * buf, unsigned bsize, int endchar ) ;
;
; Parameters:
;	char far * buf	読み込み先
;	unsigned bsize	読み取り先の領域の大きさ(バイト数)
;	int endchar	区切り文字
;
; Returns:
;	読んだ文字列の長さ
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
;	・ファイルから、endcharを読むか、bsize-1バイト読み取るまで
;	　bufに読み込みます。(endcharを読んだ場合、bufの末尾に含まれます)
;	　末尾に '\0'を追加します。
;	・buf が NULLだった場合、最大長指定つきの file_skip_until()のような
;	　動きをします。
;	・バッファリング無しの時は動作しません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial
;	92/11/25 bugfix
;	94/ 2/21 [M0.22a] bugfix (ファイルバッファの末尾がendcharだったときに
;			  見逃してしまっていた) 報告さんきう> iR
;			  bufがNULLのときも変だった(x_x)

	.MODEL SMALL
	include func.inc
	EXTRN FILE_READ:CALLMODEL

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE
func FILE_GETS		; file_gets() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+3)*2
	bsize	= (RETSIZE+2)*2
	endchar	= (RETSIZE+1)*2

	CLD
	dec	WORD PTR [BP+bsize]
	mov	DI,[BP+bsize]		; DI = rest
RLOOP:
	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	jne	short ARU

	; バッファが空だった場合、読み込む。
	sub	AX,AX
	push	AX
	push	AX
	push	AX
	_call	FILE_READ
ARU:
	mov	SI,file_InReadBuf
	sub	SI,file_BufPtr	; .InReadBuf - .BufPtr : バッファ残り
	sub	SI,DI		; DI : 読みたい長さ
	sbb	AX,AX
	and	SI,AX
	add	SI,DI
	; SI = len

	push	DI
	les	DI,file_Buffer
	add	DI,file_BufPtr
	mov	CX,SI
	mov	AL,[BP+endchar]
	repne	scasb		; endcharを捜索するのだあ
	pop	DI
	pushf			; zflagを保存
	mov	AX,SI
	sub	AX,CX

	push	[BP+buf+2]	; seg
	push	[BP+buf]	; off
	push	AX		; len
	_call	FILE_READ

	mov	BX,[BP+buf+2]
	or	BX,[BP+buf]
	jz	short NULL0
	add	[BP+buf],AX
NULL0:
	sub	DI,AX
	popf			; zflag復帰
	jz	short EXITLOOP
	test	DI,DI
	jz	short EXITLOOP
	cmp	file_Eof,0
	je	short RLOOP
EXITLOOP:
	mov	AX,[BP+buf+2]
	or	AX,[BP+buf]
	jz	short NULL
	les	BX,[BP+buf]
	mov	BYTE PTR ES:[BX],0		; '\0'を末尾に付加
NULL:
	mov	AX,[BP+bsize]
	sub	AX,DI

	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	8
endfunc			; }

END
