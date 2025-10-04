; master library - MS-DOS
;
; Description:
;	常駐データの検索( resdata_exist )
;	常駐データの作成( resdata_create )
;
; Function/Procedures:
;	unsigned resdata_exist( char * idstr, unsigned idlen, unsigned parasize) ;
;	unsigned resdata_create( char * idstr, unsigned idlen, unsigned parasize) ;
;
; Parameters:
;	char * idstr ;		/* 識別データ */
;	unsigned idlen ;	/* 識別データの長さ */
;	unsigned parasize ;	/* データブロックのパラグラフサイズ */
;
; Returns:
;	(resdata_exist)		常駐データが見つかったら、0以外を返します。
;				(常駐データのセグメントアドレス)
;	(resdata_create)	作成できない(メモリ不足)ならば 0
;				作成したなら (常駐データのセグメントアドレス)
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
;	開放は、dos_freeでやってね
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial
;	92/11/21 for Pascal
;	93/ 4/15 respal_createに戻り値をつけた
;	95/ 1/ 7 Initial: resdata.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

_mcb_flg	equ	0
_mcb_owner	equ	1
_mcb_size	equ	3

	.DATA

ResPalID	db	'pal98 grb', 0
IDLEN EQU 10

	.CODE

func RESDATA_EXIST	; resdata_exist() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引き数
	id_str	 = (RETSIZE+3)*2
	id_len	 = (RETSIZE+2)*2
	parasize = (RETSIZE+1)*2

	; 常駐パレットの捜索
	mov	AH, 52h
	int	21h
	CLD
	mov	BX, ES:[BX-2]
FIND:
	mov	ES,BX
	inc	BX
	mov	AX,ES:[_mcb_owner]
	or	AX,AX
	je	short SKIP
	mov	AX,ES:[_mcb_size]
	cmp	AX,[BP+parasize]
	jne	short SKIP
		mov	CX,[BP+id_len]
		_lds	SI,[BP+id_str]
		mov	DI,10h ; MCBの次
		rep cmpsb
		je	short FOUND
	SKIP:
	mov	AX,ES:[_mcb_size]
	add	BX,AX
	mov	AL,ES:[_mcb_flg]
	cmp	AL,'M'
	je	short FIND
NOTFOUND:
	mov	BX,0
FOUND:
	mov	AX,BX

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc			; }


func RESDATA_CREATE	; resdata_create() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	; 引き数
	id_str	 = (RETSIZE+3)*2
	id_len	 = (RETSIZE+2)*2
	parasize = (RETSIZE+1)*2

	_push	[BP+id_str+2]
	push	[BP+id_str]
	push	[BP+id_len]
	push	[BP+parasize]
	_call	RESDATA_EXIST
	or	AX,AX
	jnz	short IGNORE
CREATE:
	mov	AX,5800h	; アロケーションストラテジを得る
	int	21h
	mov	DX,AX		; 得たストラテジを保存する

	mov	AX,5801h
	mov	BX,1		; 必要最小のアロケーション
	int	21h
	mov	AH,48h		; メモリ割り当て
	mov	BX,[BP+parasize]
	int	21h
	mov	CX,0
	jc	short DAME
	mov	BX,CS		; 自分より前ならＯＫ
	cmp	BX,AX
	jnb	short ALLOC_OK
		mov	ES,AX		; 自分より後ろだったら
		mov	AH,49h		; 解放する。
		int	21h		;

		mov	AX,5801h	;
		mov	BX,2		; 最上位からのアロケーション
		int	21h

		mov	AH,48h		; メモリ割り当て
		mov	BX,[BP+parasize]
		int	21h
	ALLOC_OK:
	mov	CX,AX
	push	AX

	dec	CX			; MCBのownerを書き換える
	mov	ES,CX			;
	mov	AX,-1			;
	mov	ES:[_mcb_owner],AX	;
	inc	CX
	mov	ES,CX
	xor	DI,DI			; IDの書き込み
	mov	CX,[BP+id_len]
	_lds	SI,[BP+id_str]
	rep movsb

	pop	CX

DAME:
	mov	AX,5801h	; アロケーションストラテジの復帰
	mov	BX,DX		;
	int	21h		;
	mov	AX,CX

IGNORE:
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc			; }

END
