; master library - MS-DOS
;
; Description:
;	ファイルのタイムスタンプを変更する
;
; Function/Procedures:
;	int file_lsettime( unsigned long _filetime ) ;
;	int file_settime( unsigned _date,  unsigned _time ) ;
;
; Parameters:
;	unsigned _filetime 日時(上位=_date,下位=_time)
;			   ↑file_time()の戻り値と同じ
;	unsigned _date	   日付(MS-DOSファイル管理様式)
;	unsigned _time	   時刻(〃)
;
; Returns:
;	1    成功
;	0    失敗 (開かれていない)
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
;	実際には、クローズされるときに時刻が設定されます。
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
;	93/ 1/16 SS!=DS対応

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Handle:WORD

	.CODE

func FILE_LSETTIME
	; ラベル付けるだけで中身は完全に同じ
endfunc
func FILE_SETTIME
	mov	BX,SP
	; 引数
	_date = (RETSIZE+1)*2
	_time = (RETSIZE+0)*2
	mov	DX,SS:[BX+_date]
	mov	CX,SS:[BX+_time]
	mov	BX,file_Handle
	mov	AX,5701h
	int	21h
	sbb	AX,AX
	inc	AX
	ret	4
endfunc

END
