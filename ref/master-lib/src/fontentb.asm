; superimpose & master library module
;
; Description:
;	BFNTのANKファイルを読み取り、記憶する
;
; Funcsions/Procedures:
;	int font_entry_bfnt( char * filename ) ;
;
; Returns:
;	下のどれか。
;		NoError			成功
;		FileNotFound		ファイルがない
;		InvalidData		ファイル形式や文字の大きさが扱えない
;		InsufficientMemory	メモリが足りない
;
; Notes:
;	Heapから256文字ぶんに必要なメモリを取得します。(8x16dotなら4KB)
;	文字の大きさは、xdotsは8の倍数でかつ、(xdots/8 * ydots)が16の倍数で
;	なければ InvalidDataとして抜けます。
;	super.lib 0.22b の ank_font_load()に対応します。
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;	$Id: ankload.asm 0.03 93/01/16 14:16:40 Kazumi Rel $
;	93/ 2/24 Initial: master.lib <- super.lib 0.22b
;	93/ 8/ 7 [M0.20] 拡張ヘッダ対応
;	94/ 6/10 [M0.23] BUGFIX エラー時ファイルを閉じてなかった
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/14 [M0.22k] BUGFIX 同サイズのデータが既にあるときに死んでた

	.MODEL	SMALL
	include	super.inc
	include func.inc
	EXTRN	FONTFILE_OPEN:CALLMODEL
	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL
	EXTRN	BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_SKIP:CALLMODEL

	.DATA
	EXTRN	font_AnkSeg:WORD
	EXTRN	font_AnkSize:WORD
	EXTRN	font_AnkPara:WORD
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE
func FONT_ENTRY_BFNT	; font_entry_bfnt() {
	push	BP
	mov	BP,SP
	sub	SP,2+BFNT_HEADER_SIZE

	; 引数
	filename = (RETSIZE+1)*2

	; ローカル変数
	handle	= (-2)
	header	= (-2-BFNT_HEADER_SIZE)

	_push	[BP+filename+2]
	push	[BP+filename]
	_call	FONTFILE_OPEN
	jc	short ERROR_QUIT2
	mov	[BP+handle],AX

	push	AX
	lea	CX,[BP+header]
	push	AX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	_call	BFNT_HEADER_READ
	pop	BX
	jc	short ERROR_QUIT

	lea	CX,[BP+header]
	push	BX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	_call	BFNT_EXTEND_HEADER_SKIP
	jc	short ERROR_QUIT

	mov	AL,[BP+header].col
	cmp	AL,0			; 2色パレットなしでなければならない
	jne	short READ_ERROR

	mov	AH,byte ptr [BP+header].Xdots
	add	AH,7
	mov	CL,3
	shr	AH,CL			; Xdotsは 8で割る
	mov	AL,byte ptr [BP+header].Ydots
	test	AX,AX
	mov	DX,AX
	jz	short READ_ERROR
	cmp	font_AnkSeg,0
	je	short FORCE_ALLOC

	cmp	DX,font_AnkSize		; 現在と同じサイズであれば上書き
	mov	AX,font_AnkSeg
	je	short SKIP_ALLOCATE
	mov	AX,DX

	; in: AX=DX=size
FORCE_ALLOC:
	mul	AH
	test	AX,0f00fh		; 16の倍数でなければ駄目,
					; 256文字が64K越えるのも駄目
	jnz	short READ_ERROR
	mov	CL,4
	shr	AX,CL
	mov	font_AnkPara,AX		; 1文字に必要なパラグラフ数
	xchg	DX,AX
	mov	font_AnkSize,AX		; 1文字の大きさ
					; DX = font_AnkPara

	mov	AX,font_AnkSeg	; すでに確保されているなら開放する
	test	AX,AX
	jz	short ALLOC
	push	AX
	_call	HMEM_FREE
	jmp	short ALLOC

	; こんなとこに～～～
MEMORY:
        mov     AX,InsufficientMemory
        jmp	short ERROR_QUIT
READ_ERROR:
	mov	AX,InvalidData
ERROR_QUIT:
	push	AX
	mov	BX,[BP+handle]
	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h
	pop	AX
ERROR_QUIT2:
	stc
	jmp	short RETURN
	EVEN
ALLOC:
	mov	AH,DL	; AX=font_AnkPara * 256
	mov	mem_AllocID,MEMID_font
	mov	AL,0
	push	AX
	_call	HMEM_ALLOC
	jc	short MEMORY
	mov	font_AnkSeg,AX

	; in: AX=font_AnkSeg
SKIP_ALLOCATE:
	push	DS

	mov	BX,AX
	mov	AX,[BP+header].START
	mov	CX,[BP+header].END_
	sub	CX,AX
	inc	CX			; CX=文字数
	mov	AH,0			; AX=最初の文字番号
	mul	font_AnkPara
	add	BX,AX
	mov	AX,font_AnkPara
	mov	DS,BX

	mul	CX	; AX=文字数ぶんに必要なパラグラフ数
	mov	CL,4
	shl	AX,CL
	mov	CX,AX	; CX = 文字数ぶんに必要なバイト数
	xor	DX,DX
	mov	BX,[BP+handle]
	mov	AH,3fh		; DOS: READ HANDLE
	int	21h
	pop	DS
	jc	short READ_ERROR

	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h

	mov	AX,NoError
	clc
RETURN:
	mov	SP,BP
	pop	BP
	ret	DATASIZE*2
endfunc			; }

END
