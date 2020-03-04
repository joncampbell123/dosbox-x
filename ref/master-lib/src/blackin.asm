; superimpose & master library module
;
; Description:
;	画面を黒からじわじわと正常にする
;
; Functions/Procedures:
;	void palette_black_in( int speed ) ;
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	実行後, PaletteTone は 100になります。
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
;$Id: blackin.asm 0.01 93/02/19 20:16:20 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	93/ 9/13 [M0.21] palette_show使用(忘れてた(^^;)
;

	.MODEL SMALL
	include func.inc
	EXTRN	VSYNC_WAIT:CALLMODEL
	EXTRN	PALETTE_SHOW:CALLMODEL

	.DATA
	EXTRN Palettes : BYTE
	EXTRN PaletteTone : WORD

	.CODE

func PALETTE_BLACK_IN	; palette_black_in() {
	mov	BX,SP
	push	SI
	push	DI
	; 引数
	speed	= (RETSIZE+0)*2
	mov	SI,SS:[BX+speed]

	mov	PaletteTone,0
	call	VSYNC_WAIT	; 表示タイミングあわせ
MLOOP:
	call	PALETTE_SHOW
	mov	DI,SI
	cmp	DI,0
	jle	short SKIP
VWAIT:
	call	VSYNC_WAIT
	dec	DI
	jnz	short VWAIT
SKIP:

	add	PaletteTone,6
	cmp	PaletteTone,100
	jl	short MLOOP

	mov	PaletteTone,100
	call	PALETTE_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

; -------------- 古いやつ↓
if 0
func PALETTE_BLACK_IN
	mov	BX,SP
	push	SI
	push	DI

	; 引数
	speed	= (RETSIZE+0)*2

	mov	CH,16
	xor	DH,DH			;set data

	mov	DI,SS:[BX+speed]

NEXT_LEVEL:
	mov	CL,16
	xor	DL,DL			;color number
	mov	SI,offset Palettes

	; 16個のパレット設定
PALETTE_SET:
	mov	AL,DL
	out	0a8h,AL
	lodsw
	cmp	AL,DH
	jb	GREEN			;小さければjump
	mov	AL,DH
	out	0ach,AL			;red
GREEN:
	cmp	AH,DH
	jb	BLUE
	mov	AL,DH
	out	0aah,AL			;GREEN
BLUE:	lodsb
	cmp	AL,DH
	jb	NEXT_COLOR
	mov	AL,DH
	out	0aeh,AL			;BLUE
NEXT_COLOR:
	inc	DL
	dec	CL
	jnz	PALETTE_SET

	or	DI,DI
	jz	SKIP_VSYNC
	mov	BX,DI
VSYNC1:	
	in	AL,060h
	and	AL,020h
	jnz	short VSYNC1
VSYNC2:	
	in	AL,060h
	and	AL,020h
	jz	short VSYNC2
	dec	BX
	jnz	short VSYNC1
SKIP_VSYNC:
	inc	DH
	dec	CH
	jnz	short NEXT_LEVEL

	mov	PaletteTone,100

	pop	DI
	pop	SI
	ret	2
endfunc
endif
END
