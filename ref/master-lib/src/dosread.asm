; master library module
;
; Description:
;	ファイルの読み込み
;
; Functions/Procedures:
;	int dos_read( int fh, void far * buffer, unsigned len ) ;
;
; Parameters:
;	
;
; Returns:
;	int	AccessDenied (cy=1)	失敗,読み込み禁止
;		InvalidHandle(cy=1)	失敗,ファイルハンドルが無効
;		else	     (cy=0)	成功,読み込んだバイト数
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
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
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 3/17 Initial: master.lib
;

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_READ
	push	BP
	mov	BP,SP
	fh	= (RETSIZE+4)*2
	buf	= (RETSIZE+2)*2
	len	= (RETSIZE+1)*2

	push	DS
	mov	BX,[BP+fh]
	lds	DX,[BP+buf]
	mov	CX,[BP+len]
	mov	AH,3fh
	int	21h
	pop	DS
	jnc	short SUCCESS
	neg	AX	; cy=1
SUCCESS:
	pop	BP
	ret	8
endfunc

END
