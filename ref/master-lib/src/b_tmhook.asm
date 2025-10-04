; master library - BGM
;
; Description:
;	BGM�̃^�C�}���荞�ݓ�����
;
; Function/Procedures:
;	void interrupt far _bgm_timerhook(void);
;
; Parameters:
;
;
; Returns:
;
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V  /  PC/AT
;
; Requiring Resources:
;	CPU: V30/186
;
; Notes:
;
;
; Assembly Language Note:
;
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	femy(��  ����)		: �I���W�i���EC�����
;	steelman(���  �T�i)	: �A�Z���u�������
;	����			: PC/AT�Ή�
;
; Revision History:
;	93/12/19 Initial: b_tmhook.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 7/29 94/7/9�Ń\�[�X�������Ă����̂�94/6/21�ł����ɕ���
;	95/ 2/23 [M0.22k] RTC���荞�݃}�l�[�W��@rtc_int_set�g�p

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	timerorg:DWORD
	EXTRN	Machine_State:WORD

intdiv	dw	1

	.CODE
	EXTRN	_BGM_PLAY:CALLMODEL
	EXTRN	_BGM_EFFECT_SOUND:CALLMODEL
	EXTRN	_BGM_BELL_ORG:CALLMODEL

	public _BGM_TIMERHOOK
even
_BGM_TIMERHOOK proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX

	test	Machine_State,010h	; PC/AT?
	jz	short SET_98

	; AT�݊��@, BGM ------------------
	mov	AX,intdiv
	add	AX,4
	cmp	AX,glb.tval
	mov	intdiv,AX
	jbe	short END_AT
	mov	AX,glb.tval
	inc	AX
	sub	intdiv,AX
	jmp	short GO
	EVEN
SET_98:
	CLD

	mov	AX,glb.tval
	out	TIMER_CNT,AL		; 98
	mov	AL,AH
	out	TIMER_CNT,AL		; 98

GO:
	push	DX
	push	BX
	push	CX
	push	ES

	inc	glb.tcnt

	;�Ȑ���
	cmp	glb.tcnt,TCNTMAX
	jne	short CHECK_MANAGE_SOUND
	xor	AX,AX
	mov	glb.tcnt,AX
	cmp	glb.rflg,ON
	jne	short END_MANAGE_BGM
	push	AX
	push	PMAX
	push	AX
	call	_BGM_PLAY
	dec	AX	;cmp	AX,FINISH
	jne	short END_MANAGE_BGM
	;1�ȏI����āA���s�[�g�� OFF �Ȃ� BGM �� OFF
	cmp	glb.repsw,OFF
	jne	short END_MANAGE_BGM
	mov	glb.rflg,OFF
	call	_BGM_BELL_ORG
	jmp	short END_MANAGE_BGM
	EVEN

END_AT:
	pop	DS
	pop	AX
	retn			; @rtc_int_set��near call����邽��

	;���ʉ�����
even
CHECK_MANAGE_SOUND:
	test	glb.tcnt,3
	jne	short END_MANAGE_BGM
	cmp	glb.effect,ON
	jne	short END_MANAGE_BGM
	call	_BGM_EFFECT_SOUND
	dec	AX	;cmp	AX,FINISH
	jne	short END_MANAGE_BGM
	mov	glb.effect,OFF
	;�o�͂��I����āA�Ȃ����t����ĂȂ���Ή�������
	cmp	glb.rflg,OFF
	jne	short END_MANAGE_BGM
	call	_BGM_BELL_ORG
even
END_MANAGE_BGM:
	pop	ES
	pop	CX
	pop	BX
	pop	DX

	test	Machine_State,010h	; PC/AT?
	jnz	short END_AT
	mov	AL,EOI
	out	INTCTRL,AL		; 98
	pop	DS
	pop	AX
	iret			;***
	EVEN
_BGM_TIMERHOOK endp
END
