; master library - PC-9801
;
; Description:
;	グラフィック画面をハードウェアスクロールする
;
; Function/Procedures:
; 	void graph_scroll( unsigned line1, unsigned adr1, unsigned adr2 ) ;
;
; Parameters:
;	unsigned line1		上領域のライン数
;	unsigned adr1		上領域の先頭GDCアドレス
;	unsigned adr2		下領域の先頭GDCアドレス
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
;	CPU: 8086
;	GDC
;
; Notes:
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
;	93/ 2/ 4 Initial: grpscrol.asm
;	93/ 5/11 [M0.16] GDC 5MHzの時に異常だった(^^; bugfix
;	93/ 7/19 [M0.20] こんどはGDC 2.5MHzのときに…sz｡
;	94/ 1/22 [M0.22] graph_VramZoom対応

	.MODEL SMALL
	include func.inc

	.DATA
	extrn graph_VramLines:WORD
	extrn graph_VramZoom:WORD

	.CODE

	EXTRN	gdc_outpw:NEAR

GDCSTATUS	equ 0a0h
GDCCMD		equ 0a2h
GDCPARAM	equ 0a0h

func GRAPH_SCROLL
	push	BP
	mov	BP,SP
	; 引数
	line1	= (RETSIZE+3) * 2
	adr1	= (RETSIZE+2) * 2
	adr2	= (RETSIZE+1) * 2

	mov	BX,[BP+line1]
	mov	DX,graph_VramLines
	sub	BX,DX
	sbb	AX,AX
	and	BX,AX
	add	BX,DX	; BX = min( BX, DX )
	sub	DX,BX

	mov	CX,graph_VramZoom
	shl	BX,CL
	shl	DX,CL

	mov	CL,4
WAITEMPTY:
	jmp	$+2
	in	AL,GDCSTATUS
	test	AL,CL	; 4
	jz	short WAITEMPTY

	mov	AL,70h
	out	GDCCMD,AL

	; adr1
	mov	AX,[BP+adr1]
	call	gdc_outpw

	; line1
	mov	AX,BX
	shl	AX,CL	; CL = 4
	or	AH,CH
	call	gdc_outpw

	; adr2
	mov	AX,[BP+adr2]
	call	gdc_outpw

	; line2
	mov	AX,DX
	shl	AX,CL	; CL = 4
	or	AH,CH
	call	gdc_outpw

	pop	BP
	ret	6
endfunc

END
