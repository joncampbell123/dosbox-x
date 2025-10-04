; master library - MS-DOS
;
; Description:
;	割り込みベクタの読み取りとフック
;
; Function/Procedures:
;	void far * dos_setvect( int vect, void far * address ) ;
;
; Parameters:
;	int vect	Interrupt Number 0～255
;	void far * address 設定する割り込みルーチンのアドレス
;
; Returns:
;	void far *	INT vectに設定されていた割り込みルーチン
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
;	アセンブラから使う場合、次のように使ってください。
;	push	VECTOR		; 呼び出し
;	push	SEGMENT
;	push	OFFSET
;	call	DOS_SETVECTOR
;	mov	SAVE_SEG,DX	; 旧ベクタの退避
;	mov	SAVE_OFF,AX
;	レジスタは、AX,DX以外は保存されます。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/18 Initial
;	92/11/21 bugfix(for asm)

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_SETVECT
	push	BP
	mov	BP,SP
	push	DS

	push	BX		; アセンブラルーチンのために
	push	ES		; レジスタを保存する

	; 引数
	vect	= (RETSIZE+3)*2
	address = (RETSIZE+1)*2

	mov	AL,[BP+vect]
	lds	DX,[BP+address]

	mov	AH,35h
	int	21h		; read vector -> ES:BX
	mov	AH,25h		; set vector <- DS:DX
	int	21h

	mov	AX,BX
	mov	DX,ES

	pop	ES		; アセンブラ用に保存されたレジスタの復元
	pop	BX		;

	pop	DS
	pop	BP
	ret	6
endfunc

END
