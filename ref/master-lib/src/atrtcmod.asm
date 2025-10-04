; master library - PC/AT - RTC
;
; Description:
;	RTC���荞��(1/4096�b����)�}�l�[�W��
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
;		0�ȊO�Ȃ�Z�b�g, 0�Ȃ����
;
;	interrupt function�d�l:
;	 ���ł�CLD, push AX DS, DS=DGROUP �ɂ���Ă���
;	 near call�����
;
; Returns:
;	���W�X�^��AX,BX,CX,DX,ES���j�󂳂��
;
; Binding Target:
;	assembler
;
; Running Target:
;	PC/AT(RTC���荞�݂��g�����)
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	�Z�b�g������v���O�����I���܂łɂ��ׂĉ������邱��
;	�����̏��Ԃ͂ǂ��ł�����
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	94/ 7/ 3 Initial: atrtcmod.asm / master.lib 0.23
;	95/ 2/23 [M0.22k] RTC���荞�݃}�l�[�W��rtc_int_set�ǉ�, RTC_MOD�폜


	.186
	.MODEL SMALL
	include func.inc
	EXTRN	DOS_SETVECT:CALLMODEL

IMR		equ 0a1h	; slave IMR port
RTC_MASK	equ 1		; RTC's interrupt mask bit
RTC_VECT	equ 70h		; RTC periodic interrupt vector
RTC_INDEX	equ 70h		; RTC index port
RTC_DATA	equ 71h		; RTC data port

MAX_SLOT	equ 2		; RTC���荞�݂̓o�^�ł��鐔
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

	; �X���b�g��2�Ɍ��ߑł����Ă�
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

	;���荞�݃x�N�^�����ɖ߂�
	push	RTC_VECT
	push	word ptr CS:timerorg+2
	push	word ptr CS:timerorg
	call	DOS_SETVECT

	;�}�X�N���W�X�^�����ɖ߂�
	in	AL,IMR
	or	AL,org_rtc_mask
	out	IMR,AL
	STI
	;jmp	short WAKEUP_DONE

INT_SET_RET:
	ret			; ret�͉��ɂ������
	EVEN

	; initialize interrupt -----------------------------
INT_SET:
	CLI

	;���荞�݃x�N�^�̕ۑ�&��������
	push	RTC_VECT
	push	CS
	push	offset RTC_INT
	call	DOS_SETVECT
	mov	word ptr CS:timerorg+2,DX
	mov	word ptr CS:timerorg,AX
	;�^�C�}�C���^���v�g�����ݒ�
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

	;�}�X�N����
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
	ret			; ret�͏�ɂ������
rtc_int_set	endp

	; RTC���W�X�^�̕ύX
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
	; RTC���荞�݃n���h��
RTC_INT proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX
	CLD

	push	offset SlotEnd

	; �X���b�g��2�Ɍ��ߑł����Ă�

	; ���Ƃ�push�������[�`������Ăяo������
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
	retn	;	�����call����ƁA�Ō�ɂ�SlotEnd�ɖ߂��Ă���
	EVEN

SlotEnd:
	; ���荞�ݏI������
	cmp	org_rtc_mask,0	; �ȑO�̊��荞�݂��������Ȃ�0
	pop	DS
	jne	short SELF_EOI

	; �ȑO�̊��荞�݂�chain
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
