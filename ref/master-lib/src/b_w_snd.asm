; master library - BGM
;
; Description:
;	現在出力中の効果音が終了するまで待つ
;
; Function/Procedures:
;	int bgm_wait_sound(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		正常終了
;	BGM_NOT_PLAY		出力中ではない
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
;	femy(淀  文武)		: オリジナル・C言語版
;	steelman(千野  裕司)	: アセンブリ言語版
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
