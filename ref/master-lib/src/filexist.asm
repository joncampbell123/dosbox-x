; master library - MS-DOS
;
; Description:
;	ファイルの存在を確かめる
;
; Function/Procedures:
;	int file_exist( const char * filename ) ;
;	function file_exist( filename:string ) : boolean ;
;
; Parameters:
;	char * filename
;
; Returns:
;	1 = 存在
;	0 = 不在(またはファイルハンドルが足りないなど)
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
;	92/11/24 bugfix (thanks, Mikio!)
;	92/12/18 他のfile_*を使わない独立型に変更
;	93/ 1/16 SS!=DS対応
;       93/ 5/15 [M0.16] dos_ropen使用に変更(Turbo Pascal対応のため)

	.MODEL SMALL
	include func.inc
	EXTRN DOS_ROPEN:CALLMODEL

	.CODE
func FILE_EXIST
	mov	BX,SP
	;
	filename = (RETSIZE+0)*2

	_push	SS:[BX+filename+2]
	push	SS:[BX+filename]
	call	DOS_ROPEN
	jc	short NOTHERE
	xchg	BX,AX
	mov	AH,3eh		; クローズ
	int	21h
NOTHERE:
	sbb	AX,AX
	inc	AX
	ret	datasize*2
endfunc

END
