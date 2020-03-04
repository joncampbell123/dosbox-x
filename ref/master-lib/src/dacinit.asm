; master library - palette - VGA - 16color
;
; Description:
;	VGA 16色, ビデオDACのパレットの初期化
;
; Function/Procedures:
;	void dac_init( void ) ;
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
;	VGA DAC
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	ボーダー色は16番, (r=0,g=0,b=0), つまり黒固定で、
;	16色パレットの変更に影響をうけない
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/ 5 Initial: dacinit.asm/master.lib 0.22
;	95/ 2/ 6 [M0.22k] ボーダーカラーを黒に設定

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD
	EXTRN PalettesInit : WORD

	.CODE
	EXTRN DAC_SHOW:CALLMODEL

func DAC_INIT	; dac_init() {
	CLD
	push	SI
	push	DI
	push	DS
	pop	ES
	mov	DI,offset Palettes
	mov	SI,offset PalettesInit
	mov	CX,48/2
	rep	movsw
	pop	DI
	pop	SI
	
	mov	BL,15
PALLOOP:
	mov	BH,BL
	mov	AX,1000h	; set palette register
	int 10h
	dec	BL
	jns	short PALLOOP

	pushf
	CLI
	mov	AL,16
	mov	DX,03c8h	; video DAC
	out	DX,AL
	inc	DX
	mov	AL,0
	out	DX,AL	; r
	out	DX,AL	; g
	out	DX,AL	; b
	popf

	mov	AX,1001h	; set border color
	mov	BH,16
	int	10h

	mov	PaletteTone,100
	call	DAC_SHOW
	ret
endfunc		; }

END
