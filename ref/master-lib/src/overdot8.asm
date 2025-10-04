; master library - PC-9801V GRCG supersfx
;
; Description:
;	グラフィック画面上のドット(x, y)を含むバイトをクリアする
;	クリッピングなし
;
; Functions/Procedures:
;	void over_dot_8(int x, int y)
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
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
;	iR
;
; Revision History:
;	94/ 7/19 Initial: overdot8.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc
	.CODE

func OVER_DOT_8 	; over_dot_8() {
	push	BP
	mov	BP,SP

	x = (RETSIZE+2)*2
	y = (RETSIZE+1)*2

	; クリアするバイトのGVRAMアドレス計算
	mov	ax,0a800h
	mov	es,ax
	imul	bx,word ptr [BP+y],80
	mov	ax,[BP+x]
	shr	ax,3
	add	bx,ax
	; GRCG TDW モード
	mov	al,080h
	out	7ch,al
	; 色セット
	xor	al,al
	mov	dx,7eh
	out	dx,al
	out	dx,al
	out	dx,al
	out	dx,al
	; クリア
	mov	es:[bx],al
	; GRCG - off
	out	7ch,al

	pop	BP
	ret	4
endfunc			; }
END
