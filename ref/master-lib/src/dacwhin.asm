; superimpose & master library module
;
; Description:
;	画面を白からじわじわと正常にする
;
; Functions/Procedures:
;	void dac_white_in( speed ) ;
;
; Parameters:
;	speed	遅延時間(vsync単位) 0=遅延なし
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: whitein.asm 0.01 93/02/19 20:14:23 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/13 [M0.21] palette_show使用(忘れてた(^^;)
;	94/ 6/17 Initial: dacwhin.asm/master.lib 0.23
;

	.MODEL SMALL
	include func.inc

	.DATA
;	EXTRN Palettes:BYTE
	EXTRN PaletteTone:WORD

	.CODE
	EXTRN	VGA_VSYNC_WAIT:CALLMODEL
	EXTRN	DAC_SHOW:CALLMODEL

func DAC_WHITE_IN	; dac_white_in() {
	mov	BX,SP
	push	SI
	push	DI
	; 引数
	speed	= (RETSIZE+0)*2
	mov	SI,SS:[BX+speed]

	mov	PaletteTone,200
	call	VGA_VSYNC_WAIT	; 表示タイミングあわせ
MLOOP:
	call	DAC_SHOW
	mov	DI,SI
	cmp	DI,0
	jle	short SKIP
VWAIT:
	call	VGA_VSYNC_WAIT
	dec	DI
	jnz	short VWAIT
SKIP:

	sub	PaletteTone,6
	cmp	PaletteTone,100
	jg	short MLOOP

	mov	PaletteTone,100
	call	DAC_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

END
