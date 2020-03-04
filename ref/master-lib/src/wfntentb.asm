; master library - wfont - BFNT
;
; Description:
;	圧縮フォントのBFNTファイルからの登録
;
; Function/Procedures:
;	int wfont_entry_bfnt(const char * filename );
;
; Parameters:
;	filename  BFNTファイル名
;
; Returns:
;	下のどれか。
;		NoError			成功
;		FileNotFound		ファイルがない
;		InvalidData		ファイル形式が異なる
;		InsufficientMemory	メモリが足りない
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	Heapから2KBのメモリを取得します。
;	拡張ヘッダが存在したらスキップします。
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚
;
; Revision History:
;	93/ 7/ 2 Initial:wfntentb.asm/master.lib 0.20
;	93/ 8/ 7 [M0.20] 拡張ヘッダ対応
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/31 [M0.22k] BUGFIX 8x8dotモノクロ判定してなかった
;	95/ 3/31 [M0.22k] BUGFIX エラー時ファイルを閉じていなかった


	.MODEL SMALL
	include super.inc
	include func.inc

	.DATA
	EXTRN wfont_AnkSeg:WORD			; wfont.asm
	EXTRN mem_AllocID:WORD			; mem.asm

	.CODE
	EXTRN FONTFILE_OPEN:CALLMODEL		; fontopen.asm
	EXTRN HMEM_ALLOC:CALLMODEL		; memheap.asm
	EXTRN BFNT_HEADER_READ:CALLMODEL	; bfnthdrr.asm
	EXTRN BFNT_EXTEND_HEADER_SKIP:CALLMODEL	; bfntexts.asm

func WFONT_ENTRY_BFNT	; wfont_entry_bfnt() {
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
	call	FONTFILE_OPEN
	jc	short RETURN
	mov	[BP+handle],AX

	push	AX
	lea	CX,[BP+header]
	push	AX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	call	BFNT_HEADER_READ
	pop	BX
	jc	short HEAD_ERROR

	cmp	[BP+header].Xdots,8	; 8x8dotモノクロ以外ではInvalidData
	jne	short DATA_ERROR
	cmp	[BP+header].Ydots,8
	jne	short DATA_ERROR
	cmp	[BP+header].col,0
	jne	short DATA_ERROR

	lea	CX,[BP+header]
	push	BX		; (parameter)handle
	_push	SS
	push	CX		; (parameter)header
	call	BFNT_EXTEND_HEADER_SKIP
	jc	short HEAD_ERROR

	mov	AX,wfont_AnkSeg
	test	AX,AX
	jne	short SKIP_ALLOCATE
	mov	AX,128		; 256char * 8 bytes
	push	AX
	mov	mem_AllocID,MEMID_wfont
	call	HMEM_ALLOC
	jc	short MEMORY
	mov	wfont_AnkSeg,AX
SKIP_ALLOCATE:

	mov	BX,[BP+header].START
	mov	CX,[BP+header].END_
	sub	CX,BX
	inc	CX
	mov	BH,0
	add	AX,BX
	shl	CX,1
	shl	CX,1
	shl	CX,1

	push	DS
	mov	BX,[BP+handle]
	mov	DS,AX
	xor	DX,DX
	mov	AH,3fh		; DOS: READ HANDLE
	int	21h
	pop	DS
	jc	short DATA_ERROR

	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h

	mov	AX,NoError
	CLC
RETURN:
	mov	SP,BP
	pop	BP
	ret	DATASIZE*2

MEMORY:
        mov     AX,InsufficientMemory
        jmp	short HEAD_ERROR
DATA_ERROR:
	mov	AX,InvalidData
HEAD_ERROR:
	push	AX
	mov	BX,[BP+handle]
	mov	AH,3eh		; DOS: CLOSE HANDLE
	int	21h
	pop	AX
	STC
	jmp	short RETURN
endfunc			; }

END
