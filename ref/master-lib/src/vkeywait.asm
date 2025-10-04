; master library - key - PC/AT
;
; Description:
;	����L�[����
;
; Procedures/Functions:
;	unsigned long vkey_scan(void) ;
;	unsigned long vkey_wait(void) ;
;
; Returns:
;	bit 31: SysReq
;	bit 30: CapsLock
;	bit 29: NumLock
;	bit 28: ScrollLock
;	bit 27: right Alt
;	bit 26: right Ctrl
;	bit 25: left Alt
;	bit 24: left Ctrl
;	bit 23: Ins mode
;	bit 22: CapsLock
;	bit 21: NumLock
;	bit 20: ScrollLock
;	bit 19: Alt
;	bit 18: Ctrl
;	bit 17: left shift
;	bit 16: right shift
;
;	bit 15-8: scan key code
;	bit  7-0: key code
;
;	�Ekey_back�ɂ���Ė߂����L�[�R�[�h������΁A
;	�@���̏�ʂɌ��݂�shift�X�e�[�^�X�����ĕԂ��܂��B
;
;	�EVKEY_SCAN�ł́A�L�[���͂��Ȃ��ꍇ 0 ��Ԃ��܂��B
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Author:
;	�̂�/V
;
; Revision History:
;	93/ 8/25 Initial
;	93/ 8/29 add key_wait_at
;	94/ 4/10 Initial: vkeywait.asm/master.lib 0.23
;	94/ 4/10: key_back_buffer�Ή�

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN key_back_buffer:WORD

	.CODE

func VKEY_SCAN	; vkey_scan() {
	xor	AX,AX
	xchg	AX,key_back_buffer
	test	AX,AX
	jnz	short KEY_READ

	mov	AH,11h
	int	16h		; key ���͂���?
	jnz	short VKEY_WAIT	; ����Ȃ���ɍs��
	xor	AX,AX
	mov	DX,AX
	ret
endfunc		; }

func VKEY_WAIT	; vkey_wait() {
	xor	AX,AX
	xchg	AX,key_back_buffer
	test	AX,AX
	jnz	short KEY_READ

	mov	AH,10h		; get key
	int	16h
KEY_READ:
	mov	BX,AX
	mov	AH,12h
	int	16h		; get extend key
	mov	DX,AX
	mov	AX,BX
	ret
endfunc		; }

END
