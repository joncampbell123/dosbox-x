; master library - PC98V
;
; Description:
;	グラフィック画面を 400lineに設定する
;
; Function/Procedures:
;	void graph_400line( void ) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	同時にアクセスページを 0 に設定し、クリップ領域を画面全体にします。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial
;	93/ 3/27 gc_poly.lib合併
;	94/ 1/22 [M0.22] graph_VramZoom対応

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN graph_VramSeg : WORD
	EXTRN graph_VramWords : WORD
	EXTRN graph_VramLines : WORD
	EXTRN graph_VramZoom : WORD
	EXTRN ClipXL:WORD
	EXTRN ClipXW:WORD
	EXTRN ClipXR:WORD
	EXTRN ClipYT:WORD
	EXTRN ClipYH:WORD
	EXTRN ClipYB:WORD
	EXTRN ClipYT_seg:WORD
	EXTRN ClipYB_adr:WORD

	.CODE

func GRAPH_400LINE	; graph_400line() {
	mov	AH,42h
	mov	CH,0c0h	; 640x400,color mode,first page
	int	18h
	mov	AX,0a800h
	mov	graph_VramSeg,AX
	mov	ClipYT_Seg,AX
	mov	graph_VramWords,16000
	xor	AX,AX
	mov	ClipXL,AX
	mov	ClipYT,AX

	mov	ES,AX
	mov	AH,ES:[054dh]
	and	AH,4
	add	AH,3fh
	and	AH,40H	; GDC 2.5MHz = 00h, 5MHz = 40h
	mov	graph_VramZoom,AX	; AL=0(縦1倍)

	mov	AX,639
	mov	ClipXR,AX
	mov	ClipXW,AX
	mov	AX,400
	mov	graph_VramLines,AX
	dec	AX
	mov	ClipYB,AX
	mov	ClipYH,AX
	mov	ClipYB_adr,(80*399)
	ret
endfunc			; }

END
