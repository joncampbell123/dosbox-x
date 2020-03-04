; master library - PC98
;
; Description:
;	Palettes,PaletteToneの値を常駐パレットに書き込む
;
; Function/Procedures:
;	void respal_set_palettes( void ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	　常駐パレットがまだ検索されていない場合と、存在しない場合は
;	何もしません。
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

	.186
	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN ResPalSeg : WORD
	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD

	.CODE

func RESPAL_SET_PALETTES
	push	SI
	push	DI

	CLD
	mov	AX,ResPalSeg
	or	AX,AX
	jz	short IGNORE	; house keeping
	mov	ES,AX

	mov	AX,PaletteTone
	mov	ES:[10],AL

	mov	SI,offset Palettes
	mov	DI,16
	mov	CX,16
PLOOP:
	lodsw
	xchg	AH,AL
	shr	AX,4
	and	AL,0fh
	stosw
	lodsb
	shr	AL,4
	stosb
	loop short PLOOP
IGNORE:
	pop	DI
	pop	SI
	ret
endfunc

END
