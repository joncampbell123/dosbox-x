; master library - BGM
;
; Description:
;	���ݏo�͒��̌��ʉ����I������܂ő҂�
;
; Function/Procedures:
;	int bgm_wait_sound(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		����I��
;	BGM_NOT_PLAY		�o�͒��ł͂Ȃ�
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
;
; Revision History:
;	93/12/19 Initial: b_w_snd.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	Machine_State:WORD

	.CODE
func BGM_WAIT_SOUND
if 0
	test	byte ptr Machine_State,10h
	jz	short W98
	RTC_INDEX = 70h		; rtc index port
	RTC_DATA = 71h		; rtc data port
	CLI
	mov	AL,0ch		; register C
	out	RTC_INDEX,AL		; AT
	in	AL,RTC_DATA	; clear interrupt request
	STI
W98:
endif

	cmp	glb.effect,OFF
	jne	short WAITING
	mov	AX,BGM_NOT_PLAY
	ret
even
WAITING:
	cmp	glb.effect,ON
	je	short WAITING
	xor	AX,AX			; BGM_COMPLETE
	ret
endfunc
END
