; master library - BGM
;
; Description:
;	各種モードを設定する
;
; Function/Procedures:
;	void bgm_set_mode(int mode);
;
; Parameters:
;	mode		BGM_MUSIC と BGM_SOUND を OR してー
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V, PC/AT
;
; Requiring Resources:
;	CPU: 8086
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
;	93/12/19 Initial: b_s_mode.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 6/ 8 [M0.23] BUGFIX: 指定の禁止がまったく効いていなかった

	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
	EXTRN	BGM_STOP_SOUND:CALLMODEL
	EXTRN	BGM_STOP_PLAY:CALLMODEL

func BGM_SET_MODE	; bgm_set_mode() {
	mov	BX,SP
	mode	= (RETSIZE+0)*2
	mov	DX,SS:[BX+mode]
	mov	AX,DX
	and	AX,BGM_MUSIC	; 1
	mov	glb.music,AX
	jnz	short NOTMUSIC
	call	BGM_STOP_PLAY
	mov	glb.mcnt,0
NOTMUSIC:
	mov	AX,DX
	and	AX,BGM_SOUND_	; 2
	shr	AX,1
	mov	glb.sound,AX
	jnz	short NOTSOUND
	call	BGM_STOP_SOUND
	mov	glb.scnt,0
NOTSOUND:
	ret	2
endfunc			; }

END
