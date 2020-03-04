; master library - VGA - 16color
;
; Description:
;	VGA 16色グラフィック設定
;
; Function/Procedures:
;	int vga4_start(int videomode, int xdots, int ydots)
;	void vga4_end(void)
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
;	VGA
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
;	93/12/ 3 Initial: vgcstart.asm/master.lib 0.22
;	94/ 4/10 [M0.23] ビデオモードを backup_video_stateで覚えるようにした
;	94/ 1/ 3 [M0.23] graph_VramZoomを初期化
;	95/ 2/14 [M0.22k] ビデオモードが非消去ならdac_initせずにdac_showする
;	         ↑ちょっとやめた
;	95/ 3/28 [M0.22k] TextVramWidth,TextVramSizeも設定するようにした

	.MODEL SMALL
	include func.inc
	include vgc.inc
	EXTRN GET_MACHINE:CALLMODEL
	EXTRN GRC_SETCLIP:CALLMODEL

	.DATA
	EXTRN graph_VramSeg : WORD
	EXTRN graph_VramWords : WORD
	EXTRN graph_VramLines : WORD
	EXTRN graph_VramWidth : WORD
	EXTRN graph_VramZoom : WORD
	EXTRN VTextState : WORD
	EXTRN TextVramWidth : WORD
	EXTRN TextVramSize : WORD

	.DATA?

videostate VIDEO_STATE <?>

	.CODE
	EXTRN DAC_INIT:CALLMODEL
	EXTRN DAC_SHOW:CALLMODEL
	EXTRN SET_VIDEO_MODE:CALLMODEL
	EXTRN BACKUP_VIDEO_STATE:CALLMODEL
	EXTRN RESTORE_VIDEO_STATE:CALLMODEL

func VGA4_START	; vga4_start() {
	push	BP
	mov	BP,SP

	; 引数
	videomode	= (RETSIZE+3)*2
	xdots		= (RETSIZE+2)*2
	ydots		= (RETSIZE+1)*2

	call	GET_MACHINE		; foolproof

	_push	DS
	mov	AX,offset videostate
	push	AX
	call	BACKUP_VIDEO_STATE

	push	WORD PTR [BP+videomode]
	call	SET_VIDEO_MODE
	jc	short FAILURE

	mov	AX,[BP+videomode]
;	mov	BX,AX
	and	AL,7fh
	cmp	AX,0eh
	mov	AX,0
	jne	short NOT_200LINE
	inc	AX
NOT_200LINE:
	mov	graph_VramZoom,AX

	or	VTextState,8000h
	and	VTextState,not 04000h

;	test	BL,80h
;	jz	short protect_dac
	call	DAC_INIT
;	jmp	short end_dac
;protect_dac:
;	call	DAC_SHOW
;end_dac:

	mov	graph_VramSeg,0a000h
	mov	AX,[BP+xdots]
	add	AX,7
	mov	CL,3
	shr	AX,CL			; xを8で割る
	mov	graph_VramWidth,AX
	mov	TextVramWidth,AX	; for vtextx
	mov	BX,[BP+ydots]
	mov	graph_VramLines,BX
	mul	BX
	shr	DX,1
	rcr	AX,1
	mov	graph_VramWords,AX
	mov	CL,3
	shr	AX,CL
	mov	TextVramSize,AX		; for vtextx
	mov	AX,0
	push	AX
	push	AX
	mov	AX,[BP+xdots]
	dec	AX
	push	AX
	mov	AX,[BP+ydots]
	dec	AX
	push	AX
	call	GRC_SETCLIP

	mov	AX,1
	clc

FAILURE:
	pop	BP
	ret	6
endfunc		; }

func VGA4_END	; vga4_end() {
	and	VTextState,not 0c000h
	_push	DS
	mov	AX,offset videostate
	push	AX
	call	RESTORE_VIDEO_STATE
	ret
endfunc		; }

END
