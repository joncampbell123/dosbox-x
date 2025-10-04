; master library - VGA - VSYNC - TIMER
;
; Description:
;	VGA VSYNC���荞��
;		�J�n - vga_vsync_start
;		�I�� - vga_vsync_end
;
; Function/Procedures:
;	void vga_vsync_start(void) ;
;	void vga_vsync_end(void) ;
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Global Variables:
;	unsigned volatile vsync_Count1, vsync_Count2 ;
;		VSYNC���荞�ݖ��ɑ�����������J�E���^�B
;		vsync_start�� 0 �ɐݒ肳��܂��B
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
;	�Evga_vsync_start���s������A�K��vga_vsync_end�����s���Ă��������B
;	�@�����ӂ�ƃv���O�����I����n���O�A�b�v���܂��B
;	�Evga_vsync_start���Q�x�ȏ���s����ƁA�Q�x�ڈȍ~�̓J�E���^��
;	�@���Z�b�g���邾���ɂȂ�܂��B
;	�Evga_vsync_end���Avga_vsync_start���s�킸�Ɏ��s���Ă��������܂���B
;	�E���O��get_machine�����s����Ă���΁AWin, WinNT, OS/2�ł�
;	  ����炵�����x�ɐU�镑�����Ƃ��܂�(��)
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/21 Initial
;	93/ 2/10 vsync_Proc�p�����ǉ�
;	93/ 4/19 ���荞�݃��[�`����CLD��Y��Ă���
;	93/ 6/24 [M0.19] CLI-STI�� pushf-CLI-popf�ɕύX
;	93/ 8/ 8 [M0.20] vsync_wait�� vsyncwai.asm�ɕ���
;	93/12/11 Initial: vgavsync.asm/master.lib 0.22
;	95/ 1/30 [M0.23] vsync_Delay�ɂ��x���ǉ�
;	95/ 2/21 [M0.22k] DOS�ȊO�ŋN�����ꂽ�ꍇ�A���荞�ݎ�����������Ȃ�

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

DOSBOX	equ 8000h

;VGA_MODIFIER equ 4
VGA_MODIFIER equ 10

TIMER_VECT	EQU 08h


	.DATA
	EXTRN Machine_State:WORD

	EXTRN vsync_Count1 : WORD	; ������������J�E���^1
	EXTRN vsync_Count2 : WORD	; ������������J�E���^2
	EXTRN vsync_Proc : DWORD
	EXTRN vsync_Delay : WORD

	EXTRN vsync_Freq : WORD		; vsync�̃^�C�}�l�ɂ�銷�Z

	EXTRN vsync_OldVect : DWORD	; int 0ah(vsync)
	EXTRN vsync_OldMask : BYTE

	.DATA?
vsync_Timer	dw	?
vsync_delay_count dw ?

VGA_IN_1	equ 03dah	; VGA���̓��W�X�^1(�J���[)
VBLANK		equ 8
TIMER_COUNT0	equ 040h
TIMER_CMD	equ 043h

TILZ macro	port,mask
	local L
L:
	in	AL,port
	test	AL,mask
	jnz	short L
endm

TILNZ macro	port,mask
	local L
L:
	in	AL,port
	test	AL,mask
	jz	short L
endm


	.CODE

; VSYNC���荞�݂̏����ݒ�ƊJ�n
func VGA_VSYNC_START	; vga_vsync_start() {
	xor	AX,AX
	mov	vsync_Count1,AX
	mov	vsync_Count2,AX
	mov	vsync_delay_count,AX

	cmp	vsync_OldMask,AL ; house keeping
	jne	short S_IGNORE

	mov	vsync_Timer,AX

	CLI

	mov	AL,TIMER_VECT	; TIMER���荞�݃x�N�^�̐ݒ�ƕۑ�
	push	AX
	push	CS
	mov	AX,offset VSYNC_COUNT
	push	AX
	call	DOS_SETVECT
	mov	word ptr vsync_OldVect,AX
	mov	word ptr vsync_OldVect + 2,DX

	mov	CX,0ffffh

if 0
 	mov	AL,30h		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	out	TIMER_CMD,AL
	mov	AL,CL
	out	TIMER_COUNT0,AL	; LOW
	out	TIMER_COUNT0,AL	; HIGH, START
endif

	mov	DX,VGA_IN_1
	TILNZ	DX,VBLANK

	mov	AL,30h		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	out	TIMER_CMD,AL
	mov	AL,CL
	out	TIMER_COUNT0,AL	; LOW

	EVEN
	TILZ	DX,VBLANK

	mov	AL,CL
	out	TIMER_COUNT0,AL	; HIGH, count start

	EVEN
	TILNZ	DX,VBLANK
	TILZ	DX,VBLANK

	mov	AL,0	; latch counter#0
	out	TIMER_CMD,AL
	in	AL,TIMER_COUNT0
	sub	CL,AL
	in	AL,TIMER_COUNT0
	sbb	CH,AL		; CX
	sub	CX,4
	mov	vsync_Freq,CX

	test	Machine_State,DOSBOX
	jz	short SET_FREQ
	mov	CX,0ffffh		; DOSBOX�ł͎����͊���l0ffffh
SET_FREQ:

	TILNZ	DX,VBLANK
	mov	AL,CL
	out	TIMER_COUNT0,AL ; counter LSB
	mov	AL,CH
	out	TIMER_COUNT0,AL ; counter MSB, counter start
	STI

	mov	vsync_OldMask,1

S_IGNORE:
	ret
endfunc		; }


; INT 0ah VSYNC���荞��
VSYNC_COUNT proc far
	push	AX
	push	DX
	mov	DX,VGA_IN_1
	in	AL,DX
	push	DS

	mov	DX,seg DGROUP
	mov	DS,DX
	mov	DX,vsync_Freq

	and	AL,VBLANK	; clc
;	add	AL,-1		; stc if nonzero
	cmp	AL,1		; stc if zero
	sbb	AX,AX
public VGA_MOD
public _VGA_MOD
VGA_MOD:
_VGA_MOD:
	and	AX,VGA_MODIFIER*2
	sub	AX,VGA_MODIFIER
	add	DX,AX
	mov	AL,0	; latch counter#0
	out	TIMER_CMD,AL
	in	AL,TIMER_COUNT0	; low
	adc	DL,AL
	in	AL,TIMER_COUNT0	; high
	adc	DH,AL
	mov	AL,DL
	out	TIMER_COUNT0,AL
	mov	AL,DH
	out	TIMER_COUNT0,AL

INT_FAKE_LOOP:
	mov	AX,vsync_Delay
	add	vsync_delay_count,AX
	jc	short VSYNC_COUNT_END

	inc	vsync_Count1
	inc	vsync_Count2

	cmp	WORD PTR vsync_Proc+2,0
	je	short VSYNC_COUNT_END
	push	BX
	push	CX
	push	SI	; for pascal
	push	DI	; for pascal
	push	ES
	CLD
	call	DWORD PTR vsync_Proc
	pop	ES
	pop	DI	; for pascal
	pop	SI	; for pascal
	pop	CX
	pop	BX
	CLI
VSYNC_COUNT_END:
	mov	AX,vsync_Freq
	sub	vsync_Timer,AX
	jc	short VSYNC_COUNT_OLD

	test	Machine_State,DOSBOX
	jnz	short INT_FAKE_LOOP

	pop	DS
	pop	DX
	mov	AL,20h		; EOI
	out	20h,AL		; send EOI to master PIC
	pop	AX
	iret
	EVEN

VSYNC_COUNT_OLD:
	pushf
	call	vsync_OldVect		; �{���̃^�C�}���荞�݂����X�Ă�(��)
	pop	DS
	pop	DX
	pop	AX
	iret
	EVEN

VSYNC_COUNT endp


; VSYNC���荞�݂̏I���ƕ���
func VGA_VSYNC_END	; vga_vsync_end() {
	cmp	vsync_OldMask,0 ; house keeping
	je	short E_IGNORE

	CLI
	mov	AX,TIMER_VECT	; VSYNC���荞�݃x�N�^�̕���
	push	AX
	push	word ptr vsync_OldVect + 2
	push	word ptr vsync_OldVect
	call	DOS_SETVECT

if 0
	mov	AL,30h		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	mov	AL,0FFh
	out	TIMER_COUNT0,AL	; counter LSB
	out	TIMER_COUNT0,AL	; counter MSB, counter set
endif

	mov	AL,36h		; COUNTER#0, LSB,MSB, Mode3, BinaryCount
	out	TIMER_CMD,AL
	mov	AL,0FFh
	out	TIMER_COUNT0,AL	; counter LSB
	out	TIMER_COUNT0,AL	; counter MSB, counter start
	STI

	xor	AX,AX
	mov	vsync_OldMask,AL
	mov	vsync_Freq,AX

E_IGNORE:
	ret
endfunc		; }

END
