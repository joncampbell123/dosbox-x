; master library - BGM
;
; Description:
;	効果音を出力する
;
; Function/Procedures:
;	int bgm_sound(int num);
;
; Parameters:
;	num		効果音番号
;
; Returns:
;	BGM_COMPLETE	正常終了
;	BGM_NO_MUSIC	無効な効果音番号
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
;	93/12/19 Initial: b_sound.asm / master.lib 0.22 <- bgmlibs.lib 1.12

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	esound:WORD	;SESOUND

	.CODE
	EXTRN	_BGM_BELL_ORG:CALLMODEL

func BGM_SOUND
	mov	BX,SP
	push	SI
	num	= (RETSIZE+0)*2
	mov	SI,SS:[BX+num]
	;効果音番号チェック
	cmp	SI,1
	jl	short ILLEGAL
	cmp	SI,glb.snum
	jle	short OK
ILLEGAL:
	mov	AX,BGM_NO_MUSIC
	pop	SI
	ret	2
OK:
	cmp	glb.sound,ON
	jne	short NOSOUND
	call	_BGM_BELL_ORG
	mov	glb.scnt,SI
	;esound[glb.scnt - 1].sptr = esound[glb.scnt - 1].sbuf;
	mov	BX,SI
	shl	BX,3
	mov	AX,offset esound-8
	add	BX,AX
	mov	AX,word ptr [BX].sbuf+2
	mov	word ptr [BX].sptr+2,AX
;	mov	AX,word ptr [BX].sbuf
;	mov	word ptr [BX].sptr,AX
	mov	word ptr [BX].sptr,0	;sbufのoffsetは0のはず
	mov	glb.effect,ON
NOSOUND:
	xor	AX,AX			;AX = BGM_COMPLETE
	pop	SI
	ret	2
endfunc
END
