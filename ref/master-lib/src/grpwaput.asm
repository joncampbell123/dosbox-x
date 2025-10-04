; master library - graphic - wfont - putc - GRCG - PC98V
;
; Description:
;	グラフィック画面への圧縮半角文字描画
;
; Function/Procedures:
;	void graph_wank_putc(int x, int y, int c);
;
; Parameters:
;	x,y  描画開始座標
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
;	あらかじめ wfont_entry_bfnt()で圧縮フォントの登録が必要です。
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;
; Revision History:
;	93/ 7/ 2 Initial: grpwaput.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_Reg変数対応


	.186
	.MODEL SMALL
	include super.inc
	include func.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN	wfont_Reg:BYTE

	.CODE

func GRAPH_WANK_PUTC	; graph_wank_putc() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	ank	= (RETSIZE+1)*2

	; set color
	mov	AL,wfont_Plane1	; RMW MODE
	and	AL,wfont_Plane2
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	ES,graph_VramSeg

	mov	SI,[BP+ank]
	shl	SI,3
	mov	DS,wfont_AnkSeg

	mov	DI,[BP+y]
	mov	BX,DI
	shl	DI,2
	add	DI,BX
	shl	DI,4		; DI *= 80

	mov	CX,[BP+x]
	mov	AX,CX
	shr	AX,3
	add	DI,AX		; DI += x / 8
	and	CX,7
	jz	short NOSHIFT

	mov	DX,8
	EVEN
LINELOOP:
	mov	AH,0
	lodsb
	ror	AX,CL
	stosw
	add	DI,80-2
	dec	DX
	jnz	short LINELOOP
	jmp	short ENDDRAW

NOSHIFT:
	mov	CX,8/2
	mov	AX,80-1
	EVEN
NOSHIFTLOOP:
	movsb
	add	DI,AX
	movsb
	add	DI,AX
	loop	short NOSHIFTLOOP

ENDDRAW:
	mov	AL,0		; GRCG OFF
	out	7ch,AL

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	3*2
endfunc			; }

END
