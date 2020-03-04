; master library - MS-DOS
;
; Description:
;	ファイルポインタの移動
;	ファイルポインタの読み取り
;
; Function/Procedures:
;	void file_seek( long pos, int dir ) ;
;	unsigned long file_tell( void ) ;
;
; Parameters:
;	long pos 	位置
;	int dir		0: SEEK_SET ( ファイル先頭相対 )
;			1: SEEK_CUR ( 現在位置相対 )
;			2: SEEK_END ( ファイル末尾相対 )
;
; Returns:
;	unsigned long file_tell();	ファイルの先頭からの距離(先頭=0)
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
;	92/11/30 Initial
;	95/ 4/ 1 [M0.22k] BUGFIX file_seek エラー時に位置の把握が狂った

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_BufferPos:DWORD	; fil.asm

	EXTRN file_BufPtr:WORD		; fil.asm
	EXTRN file_Eof:WORD		; fil.asm

	.CODE
	EXTRN FILE_FLUSH:CALLMODEL	; filclose.asm

func FILE_SEEK	; file_seek() {
	call	FILE_FLUSH
	cmp	BX,-1
	je	short ERROR

	push	BP
	mov	BP,SP
	; 引数
	pos	= (RETSIZE+2)*2
	dir	= (RETSIZE+1)*2

	mov	AL,[BP+dir]
	mov	AH,42h
	mov	DX,[BP+pos]
	mov	CX,[BP+pos+2]
	int	21h
	pop	BP

	; errorがでたときのために位置を読みなおす
	mov	AX,4201h
	mov	DX,0
	mov	CX,DX
	int	21h
	mov	file_Eof,0
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,DX
ERROR:
	ret	6
endfunc		; }

func FILE_TELL	; file_tell() {
	mov	AX,file_BufPtr
	xor	DX,DX
	add	AX,WORD PTR file_BufferPos
	adc	DX,WORD PTR file_BufferPos+2
	ret
endfunc		; }

END
