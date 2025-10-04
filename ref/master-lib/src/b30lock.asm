; master library - MS-DOS - 30BIOS
;
; Description:
;	30行BIOS制御
;	常駐解除禁止，許可
;
; Procedures/Functions:
;	bios30_lock( void ) ;
;	bios30_unlock( void ) ;
;
; Parameters:
;	
;
; Returns:
;	bios30_lock:
;	bios30_unlock:
;		設定後の常駐解除禁止状態(未サポートなら常に偽)
;		偽(0)  = 常駐解除許可 / 未サポート
;		真(!0) = 常駐解除禁止
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30行BIOS 1.20以降で有効
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	ちゅ～た
;
; Advice:
;	さかきけい
;
; Revision History:
;	94/ 8/ 8  初期バージョン。
;

	.MODEL SMALL
	INCLUDE FUNC.INC
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_LOCK	; bios30_lock( void ) {
	_call	BIOS30_TT_EXIST		; 30行BIOS 1.20以降
	and	al,08h
	jz	short LOCK_FAILURE

	mov	bl,02h			; 常駐解除禁止
	mov	ax,0ff0bh
	int	18h
LOCK_FAILURE:
	ret
endfunc			; }


func BIOS30_UNLOCK	; bios30_unlock( void ) {
	_call	BIOS30_TT_EXIST		; 30行BIOS 1.20以降
	and	al,08h
	jz	short UNLOCK_FAILURE

	mov	bl,00h			; 常駐解除許可
	mov	ax,0ff0bh
	int	18h
UNLOCK_FAILURE:
	ret
endfunc			; }

	END
