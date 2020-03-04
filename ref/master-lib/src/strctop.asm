; master library
;
; Description:
;	Ｃ文字列をパスカル文字列に変換する
;
; Function/Procedures:
;	char * str_ctopas( char * PascalString, const char * CString ) ;
;
; Parameters:
;	char * PascalString	変換結果(Pascal文字列)の格納先
;	char * CString		Ｃ文字列へのアドレス
;
; Returns:
;	char *			PascalStringの値
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PascalString = CStringでも動作します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/22 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func STR_CTOPAS
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	PascalString = (RETSIZE + 1 + DATASIZE)*2
	CString	     = (RETSIZE + 1)*2

	_lds	DI,[BP+CString]
	mov	DX,DS
	mov	ES,DX

	; 長さを得る
	mov	CX,-1
	xor	AX,AX
	repne	scasb
	not	CX
	dec	CX

	lea	SI,[DI-2]

	_les	DI,[BP+PascalString]
	mov	BX,DI
	add	DI,CX

	mov	AX,CX

	STD
	rep	movsb
	stosb
	CLD

	mov	AX,BX	; DXAX = CString
	_mov	DX,ES

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret DATASIZE*4
endfunc

END
