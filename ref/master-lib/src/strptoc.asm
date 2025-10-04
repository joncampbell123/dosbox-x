; master library
;
; Description:
;	パスカル文字列をＣ文字列に変換する
;
; Function/Procedures:
;	char * str_pastoc( char * CString, const char * PascalString ) ;
;
; Parameters:
;	char * CString		変換結果(Ｃ文字列)の格納先
;	char * PascalString	Pascal文字列へのアドレス
;
; Returns:
;	char *			変換された文字列へのアドレス
;				(CStringがそのまま返される)
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
;	93/ 7/16 [M0.20] BUGFIX げ! ret で開放してる量が足りなかった(^^;;

	.MODEL SMALL
	include func.inc

	.CODE

func STR_PASTOC
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	CString      = (RETSIZE + 1 + DATASIZE)*2
	PascalString = (RETSIZE + 1)*2

	_lds	SI,[BP+PascalString]
	_les	DI,[BP+CString]
	s_mov	DX,DS
	s_mov	ES,DX
	_mov	DX,ES
	mov	BX,DI

	CLD
	xor	AX,AX	; 長さを得る
	lodsb
	mov	CX,AX

	rep	movsb
	mov	AL,0
	stosb

	mov	AX,BX	; DXAX = PascalString

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret DATASIZE*4
endfunc

END
