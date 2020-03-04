; master library - MS-DOS
;
; Description:
;	ファイルの削除
;
; Function/Procedures:
;	int file_delete( const char * filename ) ;
;	function file_delete( filename:string ) : boolean ;
;
; Parameters:
;	char * filename	ファイル名
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
;	93/ 5/15 [M0.16] dos_axdx使用
;	93/ 7/ 9 [M0.20] ひー↑でとんだ(笑)

	.MODEL SMALL
	include func.inc
	EXTRN DOS_AXDX:CALLMODEL

	.CODE
func FILE_DELETE
	mov	BX,SP
	; 引数
	filename = RETSIZE*2

	mov	AH,41h			; ファイルの削除
	push	AX
	_push	SS:[BX+filename+2]
	push	SS:[BX+filename]
	call	DOS_AXDX
	sbb	AX,AX
	inc	AX

	ret	DATASIZE*2
	EVEN
endfunc

END
