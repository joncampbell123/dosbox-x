; master library - MOUSE - MICKEY
;
; Description:
;	マウスの移動速度をミッキー／ドット比で設定する
;
; Function/Procedures:
;	void mouse_setmickey( int cx, int cy ) ;
;
; Parameters:
;	cx,cy	横と縦の、マウス座標が8移動するために必要なマウスカウント数
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	使用するマウスドライバによる
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
;	恋塚昭彦
;
; Revision History:
;	93/ 7/26 Initial: mousmicy.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN mouse_Type:WORD
	EXTRN mouse_ScaleX:WORD
	EXTRN mouse_ScaleY:WORD

	.CODE

func MOUSE_SETMICKEY ; mouse_setmickey() {
	mov	BX,SP
	; 引数
	_cx = (RETSIZE+1)*2
	_cy = (RETSIZE+0)*2

	pushf
	CLI
	mov	CX,SS:[BX+_cx]
	mov	mouse_ScaleX,CX
	mov	DX,SS:[BX+_cy]
	mov	mouse_ScaleY,DX

	cmp	mouse_Type,0
	je	short SKIP

	mov	AX,15
	int	33h
SKIP:
	popf
	ret	4
endfunc		; }

END
