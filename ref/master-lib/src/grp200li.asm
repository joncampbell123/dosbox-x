; master library - PC98
;
; Description:
;	グラフィック画面を 200lineに設定する
;
; Function/Procedures:
;	void graph_200line( int tail ) ;
;
; Parameters:
;	int	tail	0 = 上半分、 1 = 下半分
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
;	同時にアクセスページを 0 に設定します。
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

func GRAPH_200LINE
	mov	BX,SP
	mov	BX,SS:[BX+RETSIZE*2]
	mov	CX,BX
	ror	CX,1
	cmc
	rcr	CX,1
	mov	AH,42h
	int	18h

	mov	AL,8	; 高密度
	out	68h,AL

	mov	AX,0a800h
	or	BX,BX
	jz	short SKIP
	mov	AX,0abe8h
SKIP:
	mov	graph_VramSeg,AX
	mov	ClipYT_Seg,AX

	mov	AX,0
	mov	ClipXL,AX
	mov	ClipYT,AX
	mov	AX,639
	mov	ClipXR,AX
	mov	ClipXW,AX

	mov	graph_VramWords,8000
	mov	AX,200
	mov	graph_VramLines,AX
	dec	AX
	mov	ClipYB,AX
	mov	ClipYH,AX
	mov	ClipYB_adr,(80*199)
	mov	graph_VramZoom,1	; GDC 2.5MHz固定, zoom 1bit(2倍)

	ret	2
endfunc

END
