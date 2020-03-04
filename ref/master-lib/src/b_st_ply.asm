; master library - BGM
;
; Description:
;	演奏を開始する
;
; Function/Procedures:
;	int bgm_start_play(void);
;
; Parameters:
;	none
;
; Returns:
;	BGM_COMPLETE		正常終了
;	BGM_NO_MUSIC		無効な曲番号
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
;	93/12/19 Initial: b_st_ply.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB

	.CODE
	EXTRN	BGM_SELECT_MUSIC:CALLMODEL

func BGM_START_PLAY
	push	glb.mcnt
	call	BGM_SELECT_MUSIC
	mov	BX,AX
	or	BX,BX
	jne	short EXIT
	cmp	glb.music,ON
	jne	short EXIT
	mov	glb.fin,0
	mov	glb.pcnt,0
	mov	glb.repsw,ON
	mov	glb.rflg,ON
EXIT:
	ret
endfunc
END
