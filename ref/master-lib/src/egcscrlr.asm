; master library - 
;
; Description:
;	EGCによる円筒右スクロール
;
; Functions/Procedures:
;	void egc_scroll_right(int dots);
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
;$Id: rscroll.asm 0.06 92/05/28 23:00:26 Kazumi Rel $
;	94/ 5/16 Initial: egcscrlr.asm/master.lib 0.23
;

	.MODEL	SMALL
	include	func.inc

	.DATA
word_buff	DW	4 dup (?)
word_mask	DW	00000h,08000h,0c000h,0e000h,0f000h,0f800h,0fc00h,0fe00h
		DW	0ff00h,0ff80h,0ffc0h,0ffe0h,0fff0h,0fff8h,0fffch,0fffeh

	.CODE
func EGC_SCROLL_RIGHT	; egc_scroll_right() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	x	= (RETSIZE+1)*2

	push	DS
	mov	BX,[BP+x]
	mov	DI,OFFSET word_buff
	mov	SI,7cfeh
	mov	AX,DS
	mov	ES,AX
	mov	CX,16
	sub	CX,BX
	mov	AX,0a800h
	mov	DS,AX
	mov	AX,[SI]
	rol	AX,CL
	stosw
	mov	AX,0b000h
	mov	DS,AX
	mov	AX,[SI]
	rol	AX,CL
	stosw
	mov	AX,0b800h
	mov	DS,AX
	mov	AX,[SI]
	rol	AX,CL
	stosw
	mov	AX,0e000h
	mov	DS,AX
	mov	AX,[SI]
	rol	AX,CL
	stosw
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
	mov	AX,BX			; Dir/Bitアドレス
	or	AX,1000h		; Dir = 1
	out	DX,AX
	mov	DX,04aeh
	mov	AX,639			; ビット長(640-16-4)-1(?)
	out	DX,AX
	std
	mov	AX,0a800h
	mov	DS,AX
	mov	ES,AX
	mov	DX,400
	mov	SI,7cfeh
	mov	DI,7d00h
	EVEN
next_line1:	mov	CX,41
	rep	movsw
	inc	SI
	inc	SI
	inc	DI
	inc	DI
	dec	DX
	jne	next_line1
	cld
	mov	DX,04ach
	mov	AX,0000h		; Dir/Bitアドレス
	out	DX,AX
	mov	DX,04aeh
	mov	AX,BX
	dec	AX			; ビット長(640-16-4)-1(?)
	test	BX,0fh
	jnz	short no_dec
	dec	AX
	EVEN
no_dec:	out	DX,AX			; 94/05/13 16 の倍数ドットのとき
	mov	DX,400
	mov	SI,80
	mov	DI,0000h
	EVEN
next_line2:	mov	CX,1
	rep	movsw
	add	SI,78
	add	DI,78
	dec	DX
	jne	next_line2
	mov	DX,04a0h
	mov	AX,0fff0h
	out	DX,AX
	mov	AL,04h			; ExBit = 0
	out	6ah,AL
	mov	AL,00000110b		; RegWriteDisable
	out	6ah,AL
	pop	DS
	mov	AX,0c0h			; RMW mode
	out	7ch,AL
	mov	CX,16
	sub	CX,BX
	mov	DI,7d00h - 80
	mov	SI,OFFSET word_mask
	shl	BX,1
	add	SI,BX
	mov	BX,[SI]
	mov	SI,OFFSET word_buff
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	lodsw
	out	7eh,AL
	mov	AL,BH
	stosb
	mov	SI,OFFSET word_buff
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	lodsw
	mov	AL,AH
	out	7eh,AL
	mov	AL,bl
	stosb
	mov	AL,00h			; GRCG stop
	out	7ch,AL

	pop	DI
	pop	SI
	pop	BP
	ret	1*2
endfunc			; }
END
