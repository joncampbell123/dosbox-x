; superimpose & master library module
;
; Description:
;	��ʂ����킶��Ɛ^�����ɂ���
;
; Functions/Procedures:
;	void palette_black_out( int speed ) ;
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
;$Id: blackout.asm 0.01 93/02/19 20:15:27 Kazumi Rel $
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 6/25 [M0.19] palette_show�g�p

	.MODEL SMALL
	include func.inc
	EXTRN	VSYNC_WAIT:CALLMODEL
	EXTRN	PALETTE_SHOW:CALLMODEL

	.DATA
;	EXTRN	Palettes:BYTE
	EXTRN	PaletteTone:WORD

	.CODE
func PALETTE_BLACK_OUT	; palette_black_out() {
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

	sub	PaletteTone,6
	jg	short MLOOP

	mov	PaletteTone,0
	call	PALETTE_SHOW
	pop	DI
	pop	SI
	ret	2
endfunc		; }

; -------------- �Â����
if 0
func PALETTE_BLACK_OUT
	mov	BX,SP
	push	SI
	push	DI
	; ����
	speed	= (RETSIZE+0)*2

	mov	CH,16
	mov	DH,15			;set data
	mov	DI,SS:[BX+speed]

NEXT_LEVEL:
	mov	CL,16
	xor	DL,DL			;color number
	mov	SI,offset Palettes

PALETTE_SET:
	mov	AL,DL
	out	0a8h,AL
	lodsw
	cmp	AL,DH
	jb	short GREEN		;���������jump
	mov	AL,DH
	out	0ach,AL			;red
GREEN:
	cmp	AH,DH
	jb	short BLUE
	mov	AL,DH
	out	0aah,AL			;GREEN
BLUE:
	lodsb
	cmp	AL,DH
	jb	short NEXT_COLOR
	mov	AL,DH
	out	0aeh,AL			;BLUE
NEXT_COLOR:
	inc	DL
	dec	CL
	jnz	short PALETTE_SET
	or	DI,DI
	jz	short SKIP_VSYNC
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
	dec	DH
	dec	CH
	jnz	short NEXT_LEVEL

	mov	PaletteTone,0

	pop	DI
	pop	SI
	ret	2
endfunc
endif
END
