; master library - MS-DOS
;
; Description:
;	ファイルの追加オープン
;
; Function/Procedures:
;	int file_append( const char * filename ) ;
;	function file_append( filename:string ) : integer ;
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
;	ファイルが存在しなければ作成します。
;	また、バッファリングされていなければ読み書きが可能です。
;	　バッファリングしている場合、読み込みと書き込みの動作の間には
;	file_flushまたはfile_seekを実行する必要があります。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/14 Initial
;	93/ 5/15 [M0.16] pascal対応(dos_axdx使用)

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
	EXTRN DOS_AXDX:CALLMODEL

func FILE_APPEND
	push	BP
	mov	BP,SP

	; 引数
	filename = (RETSIZE+1)*2

	mov	AX,0			; FAILURE
	mov	BX,file_Handle
	cmp	BX,-1			; house keeping
	jne	short EXIT

	mov	AX,3d02h		; ファイルへの読み書きオープン
	push	AX
	_push	[BP+filename+2]
	push	[BP+filename]
	_call	DOS_AXDX
	or	AX,DX			; エラーなら -1
	mov	file_Handle,AX
	mov	CX,AX
	xor	AX,AX
	mov	file_InReadBuf,AX
	mov	file_BufPtr,AX
	mov	file_Eof,AX
	mov	file_ErrorStat,AX
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,AX
	inc	DX
	jz	short EXIT

	; 末尾へ移動
	mov	BX,CX
	xor	CX,CX
	mov	DX,CX
	mov	AX,4202h	; seek from end
	int	21h
	mov	WORD PTR file_BufferPos,AX
	mov	WORD PTR file_BufferPos+2,DX
	mov	AX,1

EXIT:	pop	BP
	ret	DATASIZE*2
endfunc

END
