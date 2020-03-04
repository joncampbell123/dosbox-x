; master library - PC98
;
; Description:
;	SDIからパレットとトーンを読み取り、Palettes,PaletteToneに設定する
;
; Function/Procedures:
;	void sdi_get_palettes( int page )
;
; Parameters:
;	int page	読み取るページ
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
;	SDIが常駐していない場合、なにもしません。
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

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN SdiExists : WORD

	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD

	.CODE

; void sdi_get_palettes( int page ) ;

func SDI_GET_PALETTES
	cmp	SdiExists,0
	jz	short IGNORE

	mov	BX,SP
	mov	AL,SS:[BX+RETSIZE*2]

	mov	AH,0Bh	; set color mode
	mov	DL,5	; 16/4096color
	int	40h

	mov	AH,0Ch	; get palettes
	mov	BX,offset Palettes
	int	40h

	mov	AH,10h	; set tone
	int	40h
	xor	DH,DH
	mov	PaletteTone,DX

IGNORE:
	ret	2
endfunc

END
