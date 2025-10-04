; master library - graphic - wfont - puts - GRCG - PC98V
;
; Description:
;	グラフィック画面への圧縮半角文字列描画
;
; Function/Procedures:
;	graph_wank_puts(int x, int y, int step, const char * str );
;
; Parameters:
;	x,y	描画座標
;	step	1文字ごとにx座標を変化させるドット数(0=進まない)
;	str	半角文字列
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
;	93/ 7/ 2 Initial: grpwapts.asm/master.lib 0.20
;	95/ 1/31 [M0.23] wfont_Reg変数対応


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD
	EXTRN	wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN	wfont_Reg:BYTE

	.CODE

func GRAPH_WANK_PUTS	; graph_wank_puts() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	step	= (RETSIZE+1+DATASIZE)*2
	anks	= (RETSIZE+1)*2

	mov	CS:ORGDSEG,DS	; DSを保存

	mov	SI,[BP+anks]

	; 色の設定
	mov	AL,wfont_Plane1	;RMW mode
	and	AL,wfont_Plane2
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	; 最初の文字は何かな
	lodsb
	or	AL,AL
	jz	short RETURN	; 文字列が空ならなにもしないぜ

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+step]
	mov	BP,BX

	mov	BX,DI		;-+
	shl	BX,2		; |
	add	DI,BX		; |DI=y*80
	shl	DI,4		;-+
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	mov	BX,0a800h
	mov	ES,BX
	mov	DX,wfont_AnkSeg
	EVEN
LOOPTOP:			; 文字列のループ
	mov	CH,8
	xor	AH,AH
	shl	AX,3
	mov	BX,AX
	mov	DS,DX		; ank seg

	EVEN

ANK_LOOP:			; 文字の描画
	mov	AL,[BX]
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,78
	inc	BX
	dec	CH
	jnz	short ANK_LOOP

	sub	DI,80 * 8
	add	CX,BP		;CH == 0!!
	mov	AX,CX
	and	CX,7
	shr	AX,3
	add	DI,AX

	mov	AX,1234h
	org $-2
ORGDSEG dw	?
	mov	DS,AX		;DS restore

	lodsb
	or	AL,AL
	jnz	short LOOPTOP

RETURN:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc			; }

END
