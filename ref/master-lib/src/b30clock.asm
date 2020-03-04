; master library - MS-DOS - 30BIOS
;
; Description:
;	30行BIOS制御
;	GDCクロックの取得，変更
;
; Procedures/Functions:
;	bios30_getclock( void ) ;
;	void bios30_setclock( int clock ) ;
;
; Parameters:
;	clock	0(BIOS30_CLOCK2) = 2.5MHz
;		1(BIOS30_CLOCK5) = 5.0MHz
;
; Returns:
;	bios30_getclock:
;		0(BIOS30_CLOCK2) = 2.5MHz / 未サポート
;		1(BIOS30_CLOCK5) = 5.0MHz
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 80186(V30)
;
; Notes:
;	30行BIOS 1.20以降で有効
;	TT.com 1.50以降で有効
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	ちゅ～た
;	さかきけい
;
; Revision History:
;	94/ 8/ 6 初期バージョン。
;

	.MODEL SMALL
	.186
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETCLOCK	; bios30_getclock( void ) {
	_call	BIOS30_TT_EXIST		; 30行BIOS 1.20以降, TT 1.50以降
	test	al,8ch
	mov	al,0
	jz	short GETCLOCK_FAILURE
	jns	short TT_GETCLOCK	; jnpは、TT 1.50以降で30行BIOS APIを
	jnp	short GETCLOCK_FAILURE	; 持たないことが前提

	mov	bl,0ffh			; 30行BIOS
	mov	ax,0ff07h
	int	18h
	xor	ah,ah
GETCLOCK_FAILURE:
	ret

TT_GETCLOCK:
	mov	ax,1802h		; TT
	mov	bx,'TT'
	int	18h
	shr	ax,2
	and	ax,1
	ret
endfunc			; }


func BIOS30_SETCLOCK	; void bios30_setclock( int clock ) {
	_call	BIOS30_TT_EXIST		; 30行BIOS 1.20以降, TT 1.50以降
	mov	bx,sp			; bx clock
	clock = (RETSIZE+0)*2
	mov	bx,ss:[bx+clock]
	and	bx,1
	test	al,8ch
	jz	short SETCLOCK_FAILURE
	jns	short TT_SETCLOCK	; jnpは、TT 1.50以降で30行BIOS APIを
	jnp	short SETCLOCK_FAILURE	; 持たないことが前提

	mov	ax,0ff07h		; 30行BIOS
	int	18h			; GDCクロック設定

	xor	bl,bl			; モードを再設定し安定させる
	mov	ax,0ff09h		; dl 拡張設定 by KEI SAKAKI.
	int	18h
	and	al,0c0h
	mov	dl,al

	mov	ah,0bh			; 設定する値の作成 by KEI SAKAKI.
	mov	bx,'30'+'行'
	int	18h
	and	al,NOT 0c0h
	or	al,dl
	dec	ah			; mov ah,0ah
TT_RETURN:
	int	18h
SETCLOCK_FAILURE:
	ret	2

TT_SETCLOCK:
	mov	dx,bx			; TT
	mov	ax,1802h
	mov	bx,'TT'
	int	18h
	and	al,NOT 04h
	shl	dl,2
	or	al,dl
	jmp	short TT_RETURN
endfunc			; }

	END
