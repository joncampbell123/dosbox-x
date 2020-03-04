; master library - MS-DOS
;
; Description:
;	ファイルのタイムスタンプを得る
;
; Function/Procedures:
;	unsigned long file_time( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	0    開かれていない
;	else 上位16bit:日付, 下位16bit:時刻(どちらもMS-DOSファイル形式)
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
;	92/12/14 Initial

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN file_Handle:WORD

	.CODE

func FILE_TIME
	mov	BX,file_Handle
	mov	AX,5700h
	int	21h
	mov	AX,CX
	cmc
	sbb	CX,CX
	and	AX,CX
	and	DX,CX
	ret
endfunc

END
