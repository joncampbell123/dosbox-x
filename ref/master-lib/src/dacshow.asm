; master library - palette - VGA - 16color
;
; Description:
;	VGA 16êF, 
;
; Function/Procedures:
;	void dac_show( void ) ;
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
;	
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
;	óˆíÀè∫ïF
;
; Revision History:
;	93/12/ 5 Initial: dacinit.asm/master.lib 0.22

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN VTextState : WORD
	EXTRN PaletteTone : WORD
	EXTRN PaletteNote : WORD
	EXTRN Palettes : WORD
	EXTRN PalettesInit : WORD

	.CODE

func DAC_SHOW	; dac_show() {
	test	VTextState,8000h	; graphic mode?
	jz	short RETURN
	CLD
	push	SI
	push	DI
	mov	AX,PaletteTone
	cwd		; if AX < 0 then AX = 0
	not	DX	;
	and	AX,DX	;
	sub	AX,200	; if AX >= 200 then AX = 200
	sbb	DX,DX	;
	and	AX,DX	;
	add	AX,200	;
	mov	CL,AL		; CL = tone

	mov	CH,100

	xor	AX,AX
	mov	BX,AX		; 0
	mov	SI,AX		; 0

	cmp	CL,CH		; 100
	jna	short SKIP
	mov	BH,3fh
	sub	CL,200		; CL = 200 - CL
	neg	CL
SKIP:
	mov	BL,byte ptr PaletteNote	; PaletteNoteÇ™0à»äOÇ»ÇÁîΩì]
	add	BL,-1
	sbb	BL,BL

	pushf
	CLI
	mov	AL,0		; from 0
	mov	DX,03c8h	; video DAC
	out	DX,AL
	inc	DX

	; AL = out value
	; AH = 
	; DX = out port
	; BL = reverse modifier
	; BH = tone modifier
	; CL = divider(100)
	; CH = multiplier
	; SI = read address(offset of 'Palettes')

PLOOP:
	mov	AX,Palettes[SI]
	shr	AX,2
	mov	DI,AX
	and	AL,3fh
	xor	AL,BH
	mul	CL
	div	CH
	xor	AL,BH
	xor	AL,BL
	out	DX,AL	; r

	mov	AX,DI
	mov	AL,AH
	xor	AL,BH
	mul	CL
	div	CH
	xor	AL,BH
	xor	AL,BL
	out	DX,AL	; g

	mov	AL,byte ptr Palettes[SI+2]
	shr	AL,2
	xor	AL,BH
	mul	CL
	div	CH
	xor	AL,BH
	xor	AL,BL
	out	DX,AL	; b

	add	SI,3
	cmp	SI,48
	jl	short PLOOP

	popf

	pop	DI
	pop	SI
RETURN:
	ret
endfunc		; }

END
