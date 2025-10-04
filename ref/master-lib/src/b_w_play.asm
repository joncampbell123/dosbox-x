; master library - BGM
;
; Description:
;	���݉��t���̋Ȃ��I������܂ő҂�
;
; Function/Procedures:
;	int bgm_wait_play(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		����I��
;	BGM_NOT_PLAY		���t���ł͂Ȃ�
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
;	93/12/19 Initial: b_w_play.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	Machine_State:WORD

	.CODE
func BGM_WAIT_PLAY
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

	cmp	glb.rflg,OFF
	jne	short PLAYING
	mov	AX,BGM_NOT_PLAY
	ret
even
PLAYING:
	mov	AX,glb.repsw
	mov	glb.repsw,OFF
even
WAITING:
	cmp	glb.rflg,ON
	je	short WAITING
	mov	glb.repsw,AX
	xor	AX,AX			; BGM_COMPLETE
	ret
endfunc
END
