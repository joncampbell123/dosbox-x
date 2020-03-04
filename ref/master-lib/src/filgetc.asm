; master library - MS-DOS
;
; Description:
;	ファイルから１文字読み込む
;
; Function/Procedures:
;	int file_getc( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	0～255	読み込んだ文字
;	-1	EOFに達している
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
;	92/11/30 BufferSize = 0 なら DOS直接

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
func FILE_GETC
	mov	AX,file_BufPtr
	cmp	file_InReadBuf,AX
	ja	short ARU

	xor	AX,AX		; バッファが空であった
	push	AX		; 読み取り先
	mov	BX,SP
	push	SS
	push	BX
	mov	AX,1
	push	AX
	call	FILE_READ
	pop	AX		; 読み取る

	cmp	file_Eof,0
	je	short EXIT
	stc
	sbb	AX,AX	; cy = 1, AX = -1
	jmp	short EXIT
ARU:
	les	BX,file_Buffer
	add	BX,file_BufPtr
	inc	file_BufPtr
	xor	AX,AX
	mov	AL,ES:[BX]
EXIT:
	ret
	EVEN
endfunc

END
