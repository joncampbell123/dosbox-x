; master library - BGM
;
; Description:
;	BGM関連内部データ
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
;	93/12/19 Initial: b_data.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	95/01/26 内部構造体を公開した(for EFSE)

	.186
	.MODEL SMALL
	include bgm.inc

	; バージョン文字列をリンクする
	EXTRN _Master_Version:BYTE

	.DATA

	.DATA?
	public	timerorg
	public	glb
	public	part
	public	esound
	public	_bgm_glb
	public	_bgm_part
	public	_bgm_esound

timerorg	dd	?
_bgm_part	label	word
part		SPART	PMAX dup(<>)
_bgm_esound	label	word
esound		SESOUND	SMAX dup(<>)

	.DATA
	public	note_dat
	public	lcount
note_dat	dw	22344, 19904, 37592, 33496, 29832, 28168, 25080, UNDF
		dw	21096, UNDF , 35488, 31608, UNDF , 26568, 23656, UNDF
		dw	23656, 21096, UNDF , 35488, 31608, UNDF , 26568, UNDF
lcount		dw	64, 32, 32, 16, 16, 16, 16,  8
		dw	 8,  8,  8,  8,  8,  8,  8,  4
		dw	 4,  4,  4,  4,  4,  4,  4,  4
		dw	 4,  4,  4,  4,  4,  4,  4,  2
		dw	 2,  2,  2,  2,  2,  2,  2,  2
		dw	 2,  2,  2,  2,  2,  2,  2,  2
		dw	 2,  2,  2,  2,  2,  2,  2,  2
		dw	 2,  2,  2,  2,  2,  2,  2,  1

_bgm_glb	label	word
glb		SGLB	<>
END
