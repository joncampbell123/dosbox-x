; master library - PC98
;
; Description:
;	常駐パレットの検索( respal_exist )
;	常駐パレットの作成( respal_create )
;
; Function/Procedures:
;	unsigned respal_exist( void ) ;
;	int respal_create( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	(respal_exist)	常駐パレットが見つかったら、0以外を返します。
;			(常駐パレットのセグメントアドレス)
;	(respal_create)	作成できない(メモリ不足)ならば 0
;			作成したなら 1
;			すでに存在したら 2
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
;	respal_createは、まだ検索を実行していない場合は、検索を行います。
;	どちらの関数とも、ResPalSeg変数に常駐パレットのセグメントアドレスを
;	格納します。
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

	.MODEL SMALL
	include func.inc

_mcb_flg	equ	0
_mcb_owner	equ	1
_mcb_size	equ	3

	.DATA

	EXTRN ResPalSeg : WORD

ResPalID	db	'pal98 grb', 0
IDLEN EQU 10

	.CODE

func RESPAL_EXIST
	push	SI
	push	DI

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
			mov	DI,10h ; MCBの次
			mov	CX,IDLEN
			mov	SI,offset ResPalID
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
	mov	ResPalSeg,AX

	pop	DI
	pop	SI
	ret
	EVEN
endfunc


func RESPAL_CREATE
	push	SI
	push	DI

	call	RESPAL_EXIST
	or	AX,AX
	mov	AX,2
	jnz	short IGNORE
CREATE:
	mov	AX,5800h	; アロケーションストラテジを得る
	int	21h
	mov	DX,AX		; 得たストラテジを保存する

	mov	AX,5801h
	mov	BX,1		; 必要最小のアロケーション
	int	21h
	mov	AH,48h		; メモリ割り当て
	mov	BX,4 ; 64/16
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
		mov	BX,4 ; 64/16
		int	21h
	ALLOC_OK:
	mov	CX,AX
	mov	ResPalSeg,AX

	dec	CX			; MCBのownerを書き換える
	mov	ES,CX			;
	mov	AX,-1			;
	mov	ES:[_mcb_owner],AX	;
	inc	CX
	mov	ES,CX
	cld
	xor	DI,DI			; IDの書き込み
	mov	SI,offset ResPalID	;
	mov	CX,IDLEN		;
	rep movsb
	xor	AX,AX
	stosw
	stosw
	stosw
	mov	CX,1

DAME:
	mov	AX,5801h	; アロケーションストラテジの復帰
	mov	BX,DX		;
	int	21h		;
	mov	AX,CX

IGNORE:
	pop	DI
	pop	SI
	ret
endfunc

END
