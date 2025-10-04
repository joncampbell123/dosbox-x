; master library - MS-DOS
;
; Description:
;	MS-DOSファンクションコール(AX=val, DS,DX=文字列)
;	(Pascal文字列)
;
; Function/Procedures:
;	long dos_axdx( int axval, void * dxstr ) ;
;	function dos_axdx( axval:integer; dxstr:string ) : LongInt ;
;
; Parameters:
;	axval	AXに格納する値
;	string	DS,DXに格納する文字列へのポインタ
;
; Returns:
;	上位16bit(DX) -1 = 失敗(cy=1), 下位16bit(AX) = DOS エラーコード
;	               0 = 成功(cy=0),               = AXの値
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
; Assembly Language Note:
;	他にも BX,CX,SI,DIを渡せます。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 5/15 Initial: dosp.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE
	EXTRN STR_PASTOC:CALLMODEL

func	DOS_AXDX	; {
	push	BP
	mov	BP,SP
	sub	SP,256
	; 引数
	axval	= (RETSIZE+1+DATASIZE)*2
	dxstr	= (RETSIZE+1)*2
	; 変数
	cstr	= -256

	push	BX
	push	CX
	_push	SS
	lea	AX,[BP+cstr]
	push	AX
	_push	[BP+dxstr+2]
	push	[BP+dxstr]
	_call	STR_PASTOC
	pop	CX
	pop	BX

	push	DS
	push	SS
	pop	DS
	lea	DX,[BP+cstr]
	mov	AX,[BP+axval]
	int	21h
	pop	DS

	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX	; エラーなら符号反転,cy=1
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
