; master library - 
;
; Description:
;	EGCによる下スクロール
;
; Functions/Procedures:
;	void egc_shift_down_all(int dots);
;
; Parameters:
;	dots
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801VX
;
; Requiring Resources:
;	CPU: 186
;	EGC
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi
;
; Revision History:
;$Id: dshifta.asm 0.02 92/05/28 22:51:14 Kazumi Rel $
;	94/ 5/16 Initial: egcsftda.asm/master.lib 0.23
;
;
;
	.186

	.MODEL	SMALL

	include	func.inc

	.CODE
func EGC_SHIFT_DOWN_ALL	; egc_shift_down_all() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	AL,00000111b		; RegWriteEnable
	out	6ah,AL
	mov	AL,80h			; CGmode = 1
	out	7ch,AL
	mov	AL,05h			; ExBit = 1
	out	6ah,AL
	mov	DX,04a0h
	mov	AX,0fff0h		; アクティブプレーン
	out	DX,AX
	mov	DX,04a2h
	mov	AX,00ffh		; FGC/BGC/リードプレーン
	out	DX,AX
	mov	DX,04a4h
	mov	AX,28f0h		; モードレジスタ/ROPコードレジスタ
	out	DX,AX
	mov	DX,04a8h
	mov	AX,0ffffh		; マスクレジスタ
	out	DX,AX
	mov	DX,04ach
	mov	AX,1000h		; dir/Bitアドレス
	out	DX,AX
	mov	DX,04aeh
	mov	AX,639			; ビット長
	out	DX,AX
	std
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	BX,400
	sub	BX,CX
	mov	SI,7d00h
	mov	DI,SI
	mov	AX,CX
	shl	AX,2
	add	AX,CX
	shl	AX,4
	sub	SI,AX
	EVEN
next_line:	mov	CX,41
	rep	movsw
	inc	SI
	inc	SI
	inc	DI
	inc	DI
	dec	BX
	jne	next_line
	cld
	mov	DX,04a0h
	mov	AX,0fff0h
	out	DX,AX
	mov	AL,04h			; ExBit = 0
	out	6ah,AL
	mov	AL,00h			; GRCG stop
	out	7ch,AL
	mov	AL,00000110b		; RegWriteDisable
	out	6ah,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	1*2
endfunc			; }
END
