; master library - PC/AT - RTC
;
; Description:
;	RTC割り込み(1/4096秒周期)マネージャ
;
; Subroutines:
;	rtc_int_set:NEAR
;
; Parameters:
;	BX = slot number
;		0: BGM
;		1: joystick
;
;	AX = offset of interrupt function(near)
;		0以外ならセット, 0なら解除
;
;	interrupt function仕様:
;	 すでにCLD, push AX DS, DS=DGROUP にされている
;	 near callされる
;
; Returns:
;	レジスタはAX,BX,CX,DX,ESが破壊される
;
; Binding Target:
;	assembler
;
; Running Target:
;	PC/AT(RTC割り込みが使える環境)
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	セットしたらプログラム終了までにすべて解除すること
;	解除の順番はどうでもいい
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 7/ 3 Initial: atrtcmod.asm / master.lib 0.23
;	95/ 2/23 [M0.22k] RTC割り込みマネージャrtc_int_set追加, RTC_MOD削除


	.186
	.MODEL SMALL
	include func.inc
	EXTRN	DOS_SETVECT:CALLMODEL

IMR		equ 0a1h	; slave IMR port
RTC_MASK	equ 1		; RTC's interrupt mask bit
RTC_VECT	equ 70h		; RTC periodic interrupt vector
RTC_INDEX	equ 70h		; RTC index port
RTC_DATA	equ 71h		; RTC data port

MAX_SLOT	equ 2		; RTC割り込みの登録できる数
	; #0 BGM
	; #1 joystick


	.DATA
InSLot		dw	0
Slot		dw	MAX_SLOT dup (0)
org_rtc_mask	db	0
org_rtc_b	db	0

	.CODE
timerorg	dd	0

	public rtc_int_set
rtc_int_set	proc near
	cmp	BX,MAX_SLOT
	jae	short INT_SET_RET		; foolproof
	shl	BX,1
	mov	Slot[BX],AX

	; スロット数2個に決め打ちしてる
	mov	AX,Slot[0]
	or	AX,Slot[2]
	add	AX,-1		; cy=1 if nonzero
	sbb	AX,AX		; AX=cy

	cmp	AX,InSLot
	je	short INT_SET_RET
	mov	InSLot,AX
	ja	short INT_SET

	; free interrupt -----------------------------------
INT_FREE:
	CLI
	mov	AL,0bh		; register B
	out	RTC_INDEX,AL
	mov	AL,org_rtc_b
	out	RTC_DATA,AL	; restore periodic interrupt mask

if 0
	mov	AL,0ch		; register C
	out	RTC_INDEX,AL
	in	AL,RTC_DATA	; clear interrupt request
endif

	;割り込みベクタを元に戻す
	push	RTC_VECT
	push	word ptr CS:timerorg+2
	push	word ptr CS:timerorg
	call	DOS_SETVECT

	;マスクレジスタを元に戻す
	in	AL,IMR
	or	AL,org_rtc_mask
	out	IMR,AL
	STI
	;jmp	short WAKEUP_DONE

INT_SET_RET:
	ret			; retは下にもあるよ
	EVEN

	; initialize interrupt -----------------------------
INT_SET:
	CLI

	;割り込みベクタの保存&書き換え
	push	RTC_VECT
	push	CS
	push	offset RTC_INT
	call	DOS_SETVECT
	mov	word ptr CS:timerorg+2,DX
	mov	word ptr CS:timerorg,AX
	;タイマインタラプト初期設定
	mov	AH,0ah		; register A
	mov	BX,0f004h	; 4096 times per sec
	call	rtc_mod

	mov	AL,0bh		; register B
	out	RTC_INDEX,AL
	in	AL,RTC_DATA
	mov	org_rtc_b,AL
	or	AL,40h		; enable periodic interrupt
	mov	AH,AL
	mov	AL,0bh		; register B
	out	RTC_INDEX,AL
	mov	AL,AH
	out	RTC_DATA,AL

	;マスク解除
	in	AL,IMR
	mov	AH,AL
	and	AL,not RTC_MASK
	out	IMR,AL
	xor	AL,AH
	mov	org_rtc_mask,AL
	STI

WAKEUP_DONE:
	CLI
	mov	AL,0ch		; register C
	out	RTC_INDEX,AL
	in	AL,RTC_DATA	; clear interrupt request
	STI
	ret			; retは上にもあるよ
rtc_int_set	endp

	; RTCレジスタの変更
rtc_mod proc near	; in: AH=index,  BH=and mask, BL=or mask
	mov	AL,AH
	out	RTC_INDEX,AL
	in	AL,RTC_DATA
	and	AL,BH
	or	AL,BL
	xchg	AH,AL
	out	RTC_INDEX,AL
	xchg	AH,AL
	out	RTC_DATA,AL
	ret
rtc_mod endp

	; --------------------------------------------------
	; RTC割り込みハンドラ
RTC_INT proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX
	CLD

	push	offset SlotEnd

	; スロット数2個に決め打ちしてる

	; あとにpushしたルーチンから呼び出されるよ
	; bgm slot
	cmp	Slot[0],0
	je	short SkipSlot0
	push	Slot[0]
SkipSlot0:
	; joystick slot
	cmp	Slot[2],0
	je	short SkipSlot1
	push	Slot[2]
SkipSlot1:
	retn	;	これでcallすると、最後にはSlotEndに戻ってくる
	EVEN

SlotEnd:
	; 割り込み終了処理
	cmp	org_rtc_mask,0	; 以前の割り込みがあったなら0
	pop	DS
	jne	short SELF_EOI

	; 以前の割り込みへchain
	pop	AX
	jmp	dword ptr CS:timerorg
	EVEN

SELF_EOI:
	mov	AL,20h
	out	0a0h,AL		; EOI to slave PIC
	out	020h,AL		; EOI to master PIC

	mov	AL,0ch
	out	RTC_INDEX,AL
	in	AL,RTC_DATA	; clear interrupt request
	mov	AL,0ch
	out	RTC_INDEX,AL
	in	AL,RTC_DATA	; clear interrupt request

	pop	AX
	iret
RTC_INT endp

END
