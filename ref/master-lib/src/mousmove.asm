; master library - cursor - moveto
;
; Description:
;	マウス座標とグラフィックカーソルを移動する
;
; Function/Procedures:
;	void mouse_cmoveto( int x, int y ) ;
;
; Parameters:
;	x,y	新しいカーソル座標
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	cursor_movetoに依存
;
; Requiring Resources:
;	CPU: cursor_movetoに依存
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
;	93/ 6/12 Initial:mousmove.asm/master.lib 0.19
;	93/ 7/ 3 [M0.20] bugfix(^^;
;	93/ 7/26 [M0.20] mousex対応
;		 ↑この変更は 0.20には入れないことにした

	.MODEL SMALL
	include func.inc


MOUSEX = 1

ifdef MOUSEX
	.DATA
	EXTRN mouse_Type:WORD
	EXTRN mouse_X:WORD
	EXTRN mouse_Y:WORD
endif

	.CODE
	EXTRN CURSOR_MOVETO:CALLMODEL
	EXTRN mouse_set_x:NEAR
	EXTRN mouse_set_y:NEAR
ifdef MOUSEX
	EXTRN mousex_snapshot:NEAR
endif

func MOUSE_CMOVETO ; mouse_cmoveto() {
	mov	BX,SP
	; 引数
	X = (RETSIZE+1)*2
	Y = (RETSIZE+0)*2

ifdef MOUSEX
	cmp	mouse_Type,0
	jne	short EXTMOUSE
endif
	pushf
	CLI
	mov	AX,SS:[BX+X]
	call	mouse_set_x
	push	AX
	mov	AX,SS:[BX+Y]
	call	mouse_set_y
	push	AX
	call	CURSOR_MOVETO
	popf
	ret	4
ifdef MOUSEX
EXTMOUSE:
	mov	AX,4
	mov	CX,SS:[BX+X]
	mov	DX,SS:[BX+Y]
	int	33h
	call	mousex_snapshot
	push	mouse_X
	push	mouse_Y
	call	CURSOR_MOVETO
	ret	4
endif
endfunc		; }

END
