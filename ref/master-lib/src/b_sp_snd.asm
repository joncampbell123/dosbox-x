; master library - BGM
;
; Description:
;	効果音を止める
;
; Function/Procedures:
;	int bgm_stop_sound(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		正常終了
;	BGM_NOT_PLAY		既に止まっている
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
;	93/12/19 Initial: b_sp_snd.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
	EXTRN	_BGM_BELL_ORG:CALLMODEL

func BGM_STOP_SOUND
	cmp	glb.effect,ON
	jne	short NOTPLAYING
	mov	glb.effect,OFF
	call	_BGM_BELL_ORG
	xor	AX,AX			; BGM_COMPLETE
	ret
even
NOTPLAYING:
	mov	AX,BGM_NOT_PLAY
	ret
endfunc
END
