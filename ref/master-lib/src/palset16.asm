; master library - 
;
; Description:
;	16ŠK’²‚ÌƒpƒŒƒbƒg‚Ì’l‚ğ 256ŠK’²‚É•ÏŠ·‚µ‚ÄPalettes‚Éİ’è‚·‚é
;
; Function/Procedures:
;	void palette_set_all_16( const void * palette ) ;
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
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
;	—ö’Ëº•F
;
; Revision History:
;	93/12/22 Initial: palset16.asm/master.lib 0.22
;	94/ 3/ 7 [M0.23] LARGEDATA BUGFIX: _pop DS–Y‚ê(^^;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes:WORD

	.CODE

func PALETTE_SET_ALL_16	; palette_set_all_16() {
	mov	BX,SP
	; ˆø”
	palette = (RETSIZE+0)*2

	push	SI
	push	DI

	push	DS
	pop	ES

	_push	DS
	_lds	SI,SS:[BX+palette]
	mov	DI,offset Palettes
	mov	CX,16
	mov	BX,17

	CLD

L:
	lodsw
	and	AX,0f0fh
	mul	BX
	stosw
	lodsb
	and	AX,0fh
	mul	BL
	stosb
	loop	short L

	_pop	DS

	pop	DI
	pop	SI
	ret	(DATASIZE)*2
endfunc		; }

END
