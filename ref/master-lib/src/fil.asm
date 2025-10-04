; master library
;
; Description:
;	ファイルアクセス関連のグローバル変数定義
;
; Global Variables:
;	file_Buffer		作業バッファのアドレス
;	file_BufferSize		作業バッファの大きさ(バイト単位)
;
;	file_BufferPos		作業バッファ先頭のファイル内位置(0=先頭)
;	file_BufPtr		作業バッファのカレントポインタ(0=Bufptr)
;	file_InReadBuf		作業バッファに読み込まれたバイト数
;
;	file_Eof		EOFに達したフラグ
;	file_ErrorStat		書き込みエラーフラグ
;
;	file_Handle		ファイルハンドル( -1=closed )
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/17 Initial
;	92/11/29 file_BufferPos追加
;	93/ 2/ 4 file_Pointer...

	.MODEL SMALL

	.DATA?

	public _file_Pointer,file_Pointer
_file_Pointer label dword
file_Pointer	dd	?

	public _file_Buffer,file_Buffer
_file_Buffer label dword
file_Buffer	dd	?

	public _file_BufferPos,file_BufferPos
_file_BufferPos label dword
file_BufferPos dd	?

	public _file_BufPtr,file_BufPtr
_file_BufPtr label word
file_BufPtr	dw	?

	public _file_InReadBuf,file_InReadBuf
_file_InReadBuf label word
file_InReadBuf	dw	?

	public _file_Eof,file_Eof
_file_Eof label word
file_Eof	dw	?

	public _file_ErrorStat,file_ErrorStat
_file_ErrorStat label word
file_ErrorStat	dw	?

	.DATA

	public _file_BufferSize,file_BufferSize
_file_BufferSize label word
file_BufferSize	dw	0

	public _file_Handle,file_Handle
_file_Handle label word
file_Handle	dw	-1

END
