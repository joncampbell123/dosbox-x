; master library - MS-DOS
;
; Description:
;	MS-DOSファンクションコール(AX=val, DS,DX=文字列)
;
; Function/Procedures:
;	long dos_axdx( int axval, void * string ) ;
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
;	93/ 5/15 Initial: dosc.asm/master.lib 0.16

	.MODEL SMALL
	include func.inc

	.CODE

func	DOS_AXDX	; {
	push	BP
	mov	BP,SP
	; 引数
	axval	= (RETSIZE+1+DATASIZE)*2
	string	= (RETSIZE+1)*2

	_push	DS
	_lds	DX,[BP+string]
	mov	AX,[BP+axval]
	int	21h
	_pop	DS
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX	; エラーなら符号反転,cy=1
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
