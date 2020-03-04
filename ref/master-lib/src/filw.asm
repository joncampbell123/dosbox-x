; master library - MS-DOS
;
; Description:
;	ファイルから１ワード読み込む
;	ファイルから１ワード書き込む
;
; Function/Procedures:
;	int file_getw( void ) ;
;	void file_putw( int i ) ;
;
; Parameters:
;	i	書き込むデータ( 下位バイト、上位バイトの順に書かれる )
;
; Returns:
;	int	(file_getw)読み込んだデータ
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
;	file_getw: 途中でEOFに遭遇した場合、読み込む値はくずれます。
;		   すでに EOFならば -1 を返しますが、どちらの場合でも、
;		   file_Eof変数でのみEOF確認を行ってください。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/25 Initial
;	93/ 1/13 そういやまだ直してなかったぜい(^^;

	.MODEL SMALL
	include func.inc
	EXTRN FILE_GETC:CALLMODEL
	EXTRN FILE_PUTC:CALLMODEL

	.CODE
func FILE_GETW
	call	FILE_GETC	; lo
	push	AX
	call	FILE_GETC	; hi
	mov	BX,AX
	pop	AX
	mov	AH,BL
	ret
	EVEN
endfunc

func FILE_PUTW
	mov	BX,SP
	; 引数
	i	= RETSIZE * 2
	mov	AX,SS:[BX+i]
	push	AX
	push	AX
	call	FILE_PUTC	; lo
	pop	AX
	mov	AL,AH
	push	AX
	call	FILE_PUTC	; hi
	ret	2
endfunc

END
