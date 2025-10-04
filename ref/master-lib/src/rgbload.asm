; superimpose & master library module
;
; Description:
;	RGBファイルの内容を Palettesに読み込む
;
; Functions/Procedures:
;	int palette_entry_rgb( const char * filename ) ;
;
; Parameters:
;	char * filename ;	RGBファイル名
;
; Returns:
;	0 = 成功
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
;	ハードウェアパレットには反映しません。
;	反映するには、この関数実行後 palette_showまたはpalette_show100を
;	呼び出して下さい。
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
;$Id: rgbload.asm 0.04 93/02/19 20:10:00 Kazumi Rel $
;
;	93/ 3/ 9 Initial: master.lib <- super.lib 0.22b
;	93/ 9/15 [M0.21] bugfix うごいてなかった(いまごろ(^^;)
;	93/12/10 [M0.22] palette幅 4bit->8bit変更に対応

	.186
	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	DOS_ROPEN:CALLMODEL

	.DATA
	EXTRN	Palettes:WORD

	.CODE

func PALETTE_ENTRY_RGB	; palette_entry_rgb() {
	mov	BX,SP

	file_name = (RETSIZE+0)*2

	_push	SS:[BX+file_name+2]
	push	SS:[BX+file_name]
	call	DOS_ROPEN
	jc	short RETURN

	mov	BX,AX			; file handle
	mov	DX,offset Palettes
	mov	CX,48
	mov	AH,03fh			; read handle
	int	21h
	sbb	CX,CX

	push	BX
	mov	BX,48-1
SHIFTLOOP:
	mov	AL,byte ptr Palettes[BX]
	shl	AL,4
	or	byte ptr Palettes[BX],AL
	dec	BX
	jns	short SHIFTLOOP
	pop	BX

	mov	AH,03eh			; close handle
	int	21h
	mov	AX,NoError
	jcxz	short RETURN
	mov	AX,InvalidData
	stc
RETURN:
	ret	DATASIZE*2
endfunc				; }

END
