; master library - MS-DOS
;
; Description:
;	ファイルへの書き込み
;
; Function/Procedures:
;	int file_write( const char far * buf, unsigned wsize ) ;
;
; Parameters:
;	char far * buf	書き込むデータの先頭アドレス
;	unsigned wsize	書き込むバイト数(0は禁止)
;
; Returns:
;	1 = 成功
;	0 = 失敗
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
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial
;	92/11/30 BufferSize = 0 ならば DOS直接に
;	92/12/08 ↑のbugfix
;	94/ 2/10 [0.22a] bugfix, バッファありのときに書き込みポインタが1しか
;			進まなかった
;	94/ 3/12 [M0.23] bugfix, バッファサイズより大きな書き込みをしたときに
;			最後の余りをバッファに書き込んだあとエラーを返していた

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Buffer:DWORD
	EXTRN file_BufferSize:WORD
	EXTRN file_BufferPos:WORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD
	EXTRN file_ErrorStat:WORD
	EXTRN file_Handle:WORD

	.CODE

func FILE_WRITE
	push	BP
	mov	BP,SP

	push	SI
	push	DI

	cmp	file_BufferSize,0
	je	short DIRECT

	; 引数
	buf	= (RETSIZE+2)*2
	wsize	= (RETSIZE+1)*2

	mov	BX,[BP+wsize]		; BX = wsize
	mov	SI,[BP+buf]		; SI = buf
WLOOP:
	mov	CX,file_BufferSize
	sub	CX,file_BufPtr
	sub	CX,BX
	sbb	AX,AX			; AX : file～が採用されてれば-1
	and	CX,AX			;      wsizeが採用されてれば 0
	add	CX,BX			; CX = writelen

	les	DI,file_Buffer
	add	DI,file_BufPtr
	sub	BX,CX			; wsize -= writelen
	add	file_BufPtr,CX		; BufPtr += writelen

	push	DS
	mov	DS,[BP+buf+2]		; カークよりエンタープライズへ
	shr	CX,1			; はい、チャーリーです
	rep	movsw			; 転送!
	adc	CX,CX
	rep	movsb			; buf += writelen
	pop	DS

	or	AX,AX
	jns	short LOOPEND

	; file～が採用されていた、すなわちバッファが満杯になった

	push	DS
	push	BX
	mov	CX,file_BufferSize
	mov	BX,file_Handle
	lds	DX,file_Buffer
	mov	AH,40h			; ファイルの書き込み
	int	21h
	pop	BX
	pop	DS
	jc	short WERROR
	cmp	file_BufferSize,AX
	jne	short WERROR

	mov	file_BufPtr,0		; 書き込んだのでバッファクリア
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0

LOOPEND:
	or	BX,BX
	jne	short WLOOP

	; Success
	mov	AX,1
	jmp	short DONE
	EVEN

DIRECT:	; DOS直接
	push	DS
	mov	CX,[BP+wsize]
	mov	BX,file_Handle
	lds	DX,[BP+buf]
	mov	AH,40h			; ファイルの書き込み
	int	21h
	pop	DS
	jnc	short EXIT

WERROR:
	mov	file_ErrorStat,1
	xor	AX,AX
EXIT:
	add	WORD PTR file_BufferPos,AX
	adc	WORD PTR file_BufferPos+2,0
	add	AX,-1
	sbb	AX,AX	; 0以外なら1にする
DONE:
	pop	DI
	pop	SI

	mov	SP,BP
	pop	BP
	ret	6
	EVEN
endfunc

END
