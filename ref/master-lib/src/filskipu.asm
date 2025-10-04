; master library - MS-DOS
;
; Description:
;	指定の文字まで読み捨てる
;
; Function/Procedures:
;	void file_skip_until( int stop ) ;
;
; Parameters:
;	int stop	停止文字(1 byte)
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
;	停止文字まで、読み捨てます。
;	見つかった場合、次に読み込めるのは停止文字の次の文字です。
;	見つからなかった場合、file_Eof = 1 になっています。
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

	.MODEL SMALL
	include func.inc
	EXTRN FILE_GETC:CALLMODEL

	.DATA
	EXTRN file_Buffer:DWORD

	EXTRN file_BufPtr:WORD
	EXTRN file_InReadBuf:WORD
	EXTRN file_Eof:WORD

	.CODE
func FILE_SKIP_UNTIL
	push	BP
	mov	BP,SP

	push	DI

	; 引数
	stop	= (RETSIZE+1)*2

	cmp	WORD PTR file_Eof,0
	jne	short EXIT
SLOOP:
	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	jbe	short EMPTY

	mov	CX,file_InReadBuf
	sub	CX,AX
	les	DI,file_Buffer
	add	DI,AX
	mov	AX,[BP+stop]
	repne	scasb			; 捜索だい
	je	short FOUND		; みつかった

EMPTY:
	mov	AX,file_InReadBuf
	mov	file_BufPtr,AX
	call	FILE_GETC
	cmp	file_Eof,0
	jne	short EXIT

	cmp	AL,[BP+stop]
	jne	short SLOOP
	; みつかった
	jmp	short EXIT
FOUND:
	sub	DI,WORD PTR file_Buffer ; 見つかった次のoffset
	mov	file_BufPtr,DI
EXIT:
	pop	DI

	mov	SP,BP
	pop	BP
	ret	2
	EVEN
endfunc

END
