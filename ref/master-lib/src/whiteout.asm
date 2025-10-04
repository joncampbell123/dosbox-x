; superimpose & master library module
;
; Description:
;	��ʂ����킶��Ɛ^�����ɂ���
;
; Functions/Procedures:
;	palette_white_out
;
; Parameters:
;	speed	�x������(vsync�P��) 0=�x���Ȃ�
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: whiteout.asm 0.01 93/02/19 20:13:01 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 6/25 [M0.19] palette_show�g�p
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Palettes:BYTE
	EXTRN PaletteTone:WORD

	.CODE

	EXTRN	VSYNC_WAIT:CALLMODEL
	EXTRN	PALETTE_SHOW:CALLMODEL

func PALETTE_WHITE_OUT	; palette_white_out {
	mov	BX,SP
	push	SI
	push	DI
	; ����
	speed	= (RETSIZE+0)*2
	mov	SI,SS:[BX+speed]

	mov	PaletteTone,100
	call	VSYNC_WAIT	; �\���^�C�~���O���킹
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
	cmp	PaletteTone,200
	jl	short MLOOP

	mov	PaletteTone,200
	call	PALETTE_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

; -------------- �Â����
if 0
func PALETTE_WHITE_OUT
	push	DI
	mov	DI,SP
	push	SI

	speed	= (RETSIZE+1)*2

	mov	DI,SS:[DI+speed]

	mov	CH,16
	xor	DH,DH			;set data
next_level:
	mov	CL,16
	xor	DL,DL			;color number
	mov	SI,offset Palettes
palette_set:
	mov	AL,DL
	out	0a8h,AL
	lodsb
	cmp	AL,DH
	ja	green			;�傫�����jump
	mov	AL,DH
	out	0ach,AL			;red
green:
	lodsb
	cmp	AL,DH
	ja	blue
	mov	AL,DH
	out	0aah,AL			;green
blue:
	lodsb
	cmp	AL,DH
	ja	next_color
	mov	AL,DH
	out	0aeh,AL			;blue
next_color:
	inc	DL
	dec	CL
	jnz	palette_set
	or	DI,DI
	jz	skip_vsync
	mov	BX,DI
vsync1:
	in	AL,060h
	and	AL,020h
	jnz	vsync1
vsync2:
	in	AL,060h
	and	AL,020h
	jz	vsync2
	dec	BX
	jnz	vsync1
skip_vsync:
	inc	DH
	dec	CH
	jnz	next_level

	mov	PaletteTone,200

	pop	SI
	pop	DI
	ret	2
endfunc
endif

END
