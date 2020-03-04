; master library - GRCG - GRAPHIC - PC-9801
;
; Description:
;	グラフィック画面への8x8dot文字描画 [色指定]
;
; Function/Procedures:
;	void graph_wank_putca( int x, int y, int ank, int color ) ;
;
; Parameters:
;	x,y	描画開始左上座標
;	ank	文字コード(0～255)
;	color	色(0～15)
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
;	Kazumi(奥田  仁)
;
; Revision History:
;	93/ 8/23 Initial: grpwapca.asm/master.lib 0.21

	.186
	.MODEL SMALL
	include super.inc
	include func.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func GRAPH_WANK_PUTCA	; graph_wank_putca() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	ank	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	DX,[BP+color]

	; GRCG setting..
	pushf
	mov	AL,0c0h		;RMW mode
	CLI
	out	7ch,AL
	popf
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
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
	ret	4*2
endfunc			; }

END
