; master library - vtext
;
; Description:
;	テキスト画面処理の開始/終了
;
; Functions/Procedures:
;	void vtext_start( void ) ;
;	void vtext_end( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	PC/AT では仮想 VRAM アドレスを返す。
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	vtext_start:
;		video mode を判定して 03 または、70 なら
;		TextVramAdr に適正値を設定します。
;		これによって video mode が 03 または 70 のときは
;		高速表示を行います。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
; Revision History:
;	93/ 8/18 Initial
;	93/ 9/21 TextVramSize , TextVramWidth を設定するよう変更
;	93/10/10 物事を実行する前に、まず get_machine っと。
;	94/01/16 text_begin -> vtext_start , pc_start
;	94/01/25 call --> _call
;	94/ 4/ 9 Initial: vtstart.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramSize:WORD
	EXTRN TextVramWidth:WORD
	EXTRN VTextState:WORD

	.CODE

	EXTRN VTEXT_WIDTH:CALLMODEL
	EXTRN VTEXT_HEIGHT:CALLMODEL

func VTEXT_START	; vtext_start() {
	_call	VTEXT_WIDTH
	mov	TextVramWidth,AX
	mov	BX,AX
	_call	VTEXT_HEIGHT
	mul	BL
	mov	TextVramSize,AX

	push	DI
	mov	AX,0b800h	; es:di = b800:0000
	mov	ES,AX
	xor	DI,DI
	mov	AH,0feh		; 仮想 VRAM address の取得
	int	10h
	mov	word ptr TextVramAdr,DI
	mov	word ptr TextVramAdr+2,ES

	and	VTextState,not 1	; updateを呼ぶ
	xor	AX,AX
	mov	ES,AX
	mov	AH,0feh
	int	10h		; 仮想 VRAM address の取得
	mov	AX,ES
	test	AX,AX
	jnz	short DONE
	or	VTextState,1	; updateを呼ばない
DONE:
	pop	DI

VTEXT_END label CALLMODEL
	ret
endfunc			; }

END
