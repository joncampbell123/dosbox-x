; master library - MS-DOS
;
; Description:
;	ファイルの新規書き込みオープン
;
; Function/Procedures:
;	int file_create( const char * filename ) ;
;
; Parameters:
;	char * filename		ファイル名
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
;	93/ 5/15 [M0.16] dos_axdx使用(pascal対応)

	.MODEL SMALL
	include func.inc
	EXTRN DOS_AXDX:CALLMODEL

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
func FILE_CREATE
	push	BP
	mov	BP,SP

	; 引数
	filename = (RETSIZE+1)*2

	mov	AX,0			; FAILURE
	mov	BX,file_Handle
	cmp	BX,-1			; house keeping
	jne	short EXIT

	mov	CX,20h			; Normal Archive File
	mov	AH,3ch			; ファイルの作成
	push	AX
	_push	[BP+filename+2]
	push	[BP+filename]
	call	DOS_AXDX
	or	AX,DX			; エラーなら -1
	mov	file_Handle,AX
	xor	AX,AX
	mov	file_InReadBuf,AX
	mov	file_BufPtr,AX
	mov	file_Eof,AX
	mov	file_ErrorStat,AX
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,AX
	mov	AX,DX
	inc	AX			; Success = 1, fault = 0

EXIT:	pop	BP
	ret	DATASIZE*2
	EVEN
endfunc

END
