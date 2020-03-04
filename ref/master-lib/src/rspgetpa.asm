; master library - PC98
;
; Description:
;	常駐パレットからパレットを読み取り、Palettes,PaletteToneに設定する
;
; Function/Procedures:
;	void respal_get_palettes( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	none
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
;	　常駐パレットが存在しない場合は何もしません。
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
;	93/12/10 [M0.22] パレット幅 4bit->8bitに対応

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN ResPalSeg : WORD
	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD

	.CODE
	EXTRN RESPAL_EXIST : CALLMODEL

func RESPAL_GET_PALETTES
	call	RESPAL_EXIST
	or	AX,AX
	jz	short IGNORE	; house keeping

	push	SI
	push	DI
	push	DS
	push	DS
	pop	ES
	mov	DS,AX

	mov	AL,DS:[10]	; tone
	xor	AH,AH
	mov	ES:PaletteTone,AX

	mov	SI,16
	mov	DI,offset Palettes
	mov	CX,SI
	mov	BX,17
PLOOP:
	lodsw
	xchg	AH,AL
	and	AX,0f0fh
	mul	BX
	stosw
	lodsb
	mul	BL
	stosb
	loop short PLOOP

	pop	DS
	pop	DI
	pop	SI
IGNORE:
	ret
endfunc

END
