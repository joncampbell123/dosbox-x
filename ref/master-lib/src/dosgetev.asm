; master library - MS-DOS
;
; Description:
;	環境変数の取得
;
; Function/Procedures:
;	const char far * dos_getenv( unsigned envseg, const char * envname ) ;
;
; Parameters:
;	unsigned envseg	環境変数領域の先頭のセグメントアドレス
;			0ならば自分の環境変数
;	char * envname	環境変数名(英字は大文字にすること)
;
; Returns:
;	char far *	見つかった環境変数の内容文字列へのポインタ
;			見つからなければ NULL
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
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/26 Initial
;	93/ 5/30 [M0.18] envseg = 0ならば自分の環境変数を見るようにした

	.MODEL SMALL
	include func.inc

	.CODE

func DOS_GETENV
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引数
	envseg	= (RETSIZE+1+DATASIZE)*2
	envname	= (RETSIZE+1)*2

	_lds	DI,[BP+envname]	; DS = FP_SEG(envname)
	mov	DX,DI		; DX = FP_OFF(envname)
	mov	BX,DS
	mov	ES,BX

	mov	CX,-1		; 指定環境変数名の長さを得る
	xor	AX,AX
	repne	scasb
	not	CX
	dec	CX

	mov	BX,[BP+envseg]
	test	BX,BX
	jnz	short GO
	mov	AH,62h		; get PSP
	int	21h
	mov	ES,BX
	mov	BX,ES:[2ch]
	xor	AX,AX
GO:
	mov	ES,BX
	mov	DI,AX		; 0
	mov	BX,CX		; BX = 長さ

	cmp	ES:[DI],AL
	je	short NOTFOUND
	EVEN
SLOOP:
	mov	AL,'='		; 環境変数の名前の長さを取り出す
	mov	CX,-1
	repne	scasb
	jne	short NOTFOUND	; house keeping
	not	CX
	dec	CX
	cmp	CX,BX		; 長さが一致しなければ次へスキップ
	jne	short NEXT

	mov	SI,DX		; 名前の比較
	sub	DI,CX
	dec	DI
	repe	cmpsb
	je	short FOUND

NEXT:	; 次の環境変数名を探す
	xor	AX,AX
	mov	CX,-1
	repne	scasb
	jne	short NOTFOUND	; house keeping
	cmp	ES:[DI],AL
	jne	short SLOOP

NOTFOUND:			; 見つからない
	xor	AX,AX
	mov	DX,AX
	jmp	short RETURN
	EVEN

FOUND:				; 見つかった
	mov	DX,ES
	mov	AX,DI
	inc	AX

RETURN:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
