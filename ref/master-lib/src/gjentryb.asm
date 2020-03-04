; master library - PC98
;
; Description:
;	外字のBFNTファイルからの登録
;
; Function/Procedures:
;	int gaiji_entry_bfnt( const char * filename ) ;
;
; Parameters:
;	const char * filename	BNFTファイル名
;
; Returns:
;	int	1 = 成功
;		0 = 失敗(メモリ(8KB)足りねえ/ファイルが開けない/
;		         指定した形のBFNTぢゃない)
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
;	モノクロ, 16x16dot, 0～255ぢゃないとやだ
;	拡張へっだはあってもいいよ
;	BFNT1.6の仕様ならいいよ
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 2/ 4 Initial
;	93/ 5/ 9 [M0.16] super.lib部を使用

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
	EXTRN	FONTFILE_OPEN:CALLMODEL
	EXTRN	BFNT_HEADER_READ:CALLMODEL
	EXTRN	BFNT_EXTEND_HEADER_SKIP:CALLMODEL

GAIJISIZE = 256*2*16

	.DATA

bfnt_header2 dw 16,16,0,255

	.CODE

	EXTRN	GAIJI_WRITE_ALL:CALLMODEL

func	GAIJI_ENTRY_BFNT	; {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	sub	SP,BFNT_HEADER_SIZE

	; 引数
	filename = (RETSIZE+1)*2

	_push	[BP+filename+2]
	push	[BP+filename]
	call	FONTFILE_OPEN
	mov	BP,AX		; BP = file handle
	mov	AX,0
	jc	short	E_OPEN

	mov	AX,GAIJISIZE
	push	AX
	call	SMEM_WGET
	mov	SI,AX		; SI = バッファのセグメント
	mov	AX,0
	jc	short E_MEMORY

	mov	AX,SP
	push	BP
	_push	SS
	push	AX
	call	BFNT_HEADER_READ
	jc	short DAME

	mov	DI,SP
	push	SS
	pop	ES
	cmp	byte ptr ES:[DI+5],0	; col
	jne	short DAME
	add	DI,8	; Xdots
	push	SI
	mov	SI,offset bfnt_header2
	mov	CX,4
	repe	cmpsw
	pop	SI
	jne	short DAME

	mov	AX,SP
	push	BP
	_push	SS
	push	AX
	call	BFNT_EXTEND_HEADER_SKIP

	push	DS
	mov	DS,SI
	mov	BX,BP
	xor	DX,DX
	mov	CX,GAIJISIZE
	mov	AH,3fh
	int	21h		; データ部の読み込み
	pop	DS
	cmp	AX,GAIJISIZE
	jne	short DAME

	push	SI
	xor	AX,AX
	push	AX
	call	GAIJI_WRITE_ALL

	; 成功
OK:
	mov	AX,1
	jmp	short OWARI
DAME:
	xor	AX,AX
OWARI:
	push	AX
	push	SI
	call	SMEM_RELEASE
	pop	AX

E_MEMORY:
	push	AX
	mov	BX,BP
	mov	AH,3eh
	int	21h
	pop	AX
E_OPEN:
	add	SP,BFNT_HEADER_SIZE
	pop	DI
	pop	SI
	pop	BP
	ret	DATASIZE*2
endfunc				; }

END
