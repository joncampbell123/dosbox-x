; master library - VGA - VSYNC - TIMER
;
; Description:
;	VGA VSYNC割り込み
;		開始 - vga_vsync_start
;		終了 - vga_vsync_end
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
;		VSYNC割り込み毎に増加し続けるカウンタ。
;		vsync_startで 0 に設定されます。
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
;	・vga_vsync_startを行ったら、必ずvga_vsync_endを実行してください。
;	　これを怠るとプログラム終了後ハングアップします。
;	・vga_vsync_startを２度以上実行すると、２度目以降はカウンタを
;	　リセットするだけになります。
;	・vga_vsync_endを、vga_vsync_startを行わずに実行しても何もしません。
;	・事前にget_machineが実行されていれば、Win, WinNT, OS/2でも
;	  それらしい速度に振る舞おうとします(笑)
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/21 Initial
;	93/ 2/10 vsync_Proc用処理追加
;	93/ 4/19 割り込みルーチンでCLDを忘れていた
;	93/ 6/24 [M0.19] CLI-STIを pushf-CLI-popfに変更
;	93/ 8/ 8 [M0.20] vsync_waitを vsyncwai.asmに分離
;	93/12/11 Initial: vgavsync.asm/master.lib 0.22
;	95/ 1/30 [M0.23] vsync_Delayによる遅延追加
;	95/ 2/21 [M0.22k] DOS以外で起動された場合、割り込み周期をいぢらない

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

DOSBOX	equ 8000h

;VGA_MODIFIER equ 4
VGA_MODIFIER equ 10

TIMER_VECT	EQU 08h


	.DATA
	EXTRN Machine_State:WORD

	EXTRN vsync_Count1 : WORD	; 増加し続けるカウンタ1
	EXTRN vsync_Count2 : WORD	; 増加し続けるカウンタ2
	EXTRN vsync_Proc : DWORD
	EXTRN vsync_Delay : WORD

	EXTRN vsync_Freq : WORD		; vsyncのタイマ値による換算

	EXTRN vsync_OldVect : DWORD	; int 0ah(vsync)
	EXTRN vsync_OldMask : BYTE

	.DATA?
vsync_Timer	dw	?
vsync_delay_count dw ?

VGA_IN_1	equ 03dah	; VGA入力レジスタ1(カラー)
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

; VSYNC割り込みの初期設定と開始
func VGA_VSYNC_START	; vga_vsync_start() {
	xor	AX,AX
	mov	vsync_Count1,AX
	mov	vsync_Count2,AX
	mov	vsync_delay_count,AX

	cmp	vsync_OldMask,AL ; house keeping
	jne	short S_IGNORE

	mov	vsync_Timer,AX

	CLI

	mov	AL,TIMER_VECT	; TIMER割り込みベクタの設定と保存
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
	mov	CX,0ffffh		; DOSBOXでは周期は既定値0ffffh
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


; INT 0ah VSYNC割り込み
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
	call	vsync_OldVect		; 本来のタイマ割り込みも時々呼ぶ(笑)
	pop	DS
	pop	DX
	pop	AX
	iret
	EVEN

VSYNC_COUNT endp


; VSYNC割り込みの終了と復元
func VGA_VSYNC_END	; vga_vsync_end() {
	cmp	vsync_OldMask,0 ; house keeping
	je	short E_IGNORE

	CLI
	mov	AX,TIMER_VECT	; VSYNC割り込みベクタの復元
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
