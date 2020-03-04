; master library - PC/AT, VGA
;
; Description:
;	PC/ATで98の400lineを擬似的に実現する
;
; Functions/Procedures:
;	void at98_graph_400line(void);
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT+VGA
;
; Requiring Resources:
;	CPU: 186
;	VGA 16color
;
; Notes:
;	表示mode = 0eh を縦2倍解除 (640x400x16color)
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
;	94/ 7/ 5 Initial: at98g400.asm/master.lib 0.23
;	95/ 2/ 6 [M0.22k] 画面を消去しないようにし、ボーダーカラー設定を
;			dac_initに移動。

	.186
	.MODEL SMALL
	include func.inc

	EXTRN GET_MACHINE:CALLMODEL

DISPLAY_480LINE equ 0
if DISPLAY_480LINE eq 0
	.DATA
	EXTRN graph_VramZoom:WORD
endif

	.DATA
	EXTRN at98_APage:WORD
	EXTRN at98_VPage:WORD
	EXTRN at98_Offset:WORD

	.CODE
	EXTRN VGA4_START:CALLMODEL
	EXTRN VGA_SETLINE:CALLMODEL
	EXTRN AT98_SHOWPAGE:CALLMODEL
	EXTRN AT98_ACCESSPAGE:CALLMODEL
	EXTRN VTEXTX_START:CALLMODEL


if DISPLAY_480LINE eq 0
	EXTRN VGA_DC_MODIFY:CALLMODEL
endif

func AT98_GRAPH_400LINE	; at98_graph_400line() {
	call	GET_MACHINE
if DISPLAY_480LINE ne 0
	push	(012h+80h)
	push	640
	push	400
	call	VGA4_START
	push	400
	call	VGA_SETLINE
else
	push	(0eh+80h)
	push	640
	push	400
	call	VGA4_START
	push	9
	push	7fh
	push	0
	call	VGA_DC_MODIFY
	mov	graph_VramZoom,0
endif

	mov	at98_Offset,0
	push	at98_VPage
	call	AT98_SHOWPAGE
	push	at98_APage
	call	AT98_ACCESSPAGE
	call	VTEXTX_START
	ret
endfunc		; }

END
