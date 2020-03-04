; master library - MS-DOS
;
; Description:
;	ファイルからの読み込み
;
; Function/Procedures:
;	unsigned file_read( char far * buf, unsigned bsize ) ;
;
; Parameters:
;	char far * buf	読み込み先( NULLならば、bsize分読み捨てる )
;	unsigned bsize	最大読み取り長(バイト数,
;			0 ならば file_Bufferを必要ならば更新するだけ )
;
; Returns:
;	読み込んだバイト数
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
;	file_EOFは、1バイトも読めなかった時だけ設定される。
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
;	92/11/30 BufferSize = 0なら DOS直接に
;	92/12/06 ↑で発生した、[indirect:ぜんぜん動かん][direct:retcode異常]
;		 を修正。

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE

func FILE_READ
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+2)*2
	bsize	= (RETSIZE+1)*2

	cmp	file_BufferSize,0
	je	short DIRECT

	mov	BX,[BP+bsize]		; BX = rest
	les	DI,[BP+buf]
RLOOP:
	mov	AX,file_InReadBuf
	cmp	file_BufPtr,AX
	jb	short MADA_ARU

	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0

	push	BX
	push	DS
	mov	CX,file_BufferSize
	mov	BX,file_Handle
	lds	DX,file_Buffer
	mov	AH,3Fh			; ファイルの読み込み
	int	21h
	pop	DS
	pop	BX
	cmc				; エラーなら AX = 0 -> EOF
	sbb	DX,DX
	and	AX,DX
	mov	file_InReadBuf,AX
	jz	short EOF

	mov	file_BufPtr,0		; 読めた
MADA_ARU:
	mov	SI,file_InReadBuf	; SI = len
	sub	SI,file_BufPtr
	sub	SI,BX
	sbb	AX,AX
	and	SI,AX
	add	SI,BX
	mov	AX,ES
	or	AX,DI
	je	short LOOPEND
	or	SI,SI
	je	short LOOPEND

	push	SI			; 転送
	push	DS
	mov	CX,SI
	mov	AX,file_BufPtr
	lds	SI,file_Buffer
	add	SI,AX
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	pop	DS
	pop	SI
LOOPEND:
	add	file_BufPtr,SI
	sub	BX,SI
	jne	short RLOOP
	jmp	short EXITLOOP
	EVEN
	; バッファサイズが0ならDOS直接アクセス
DIRECT:
	push	DS
	mov	CX,[BP+bsize]		; BX = rest

	mov	BX,file_Handle
	lds	DX,[BP+buf]
	mov	AH,3Fh			; ファイルの読み込み
	int	21h
	pop	DS
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0
	mov	BX,CX
	sub	BX,AX
	je	short EXITLOOP
EOF:
	mov	file_Eof,1
EXITLOOP:
	mov	AX,[BP+bsize]
	sub	AX,BX

	pop	DI
	pop	SI
	pop	BP
	ret	6
	EVEN
endfunc

END
