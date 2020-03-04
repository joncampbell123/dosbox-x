; master library - VGA - 16color
;
; Description:
;	VGA 16色, グラフィック画面の任意の地点の色番号を得る
;
; Function/Procedures:
;	int vga4_readdot( int x, int y ) ;
;
; Parameters:
;	int	x	ｘ座標(0～639)
;	int	y	ｙ座標(0～479)
;
; Returns:
;	int	色コード(0～15)または、ｘ座標が範囲外ならば -1
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
;	現在の画面サイズでクリッピングしてます。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/ 3 Initial vgcrddot.asm/master.lib 0.22
;	94/ 2/ 7 [M0.22a] bugfix
;	94/ 4/14 [M0.23] vga4_readdot()に改名

	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramLines:WORD
	EXTRN graph_VramWidth:WORD

	.CODE

func VGA4_READDOT	; vga4_readdot() {
	mov	BX,SP
	; 引数
	x	= (RETSIZE+1)*2
	y	= (RETSIZE+0)*2

	mov	AX,-1

	mov	CX,SS:[BX+y]
	cmp	CX,graph_VramLines
	jnb	short IGNORE

	mov	DX,graph_VramWidth
	shl	DX,1
	shl	DX,1
	shl	DX,1

	mov	BX,SS:[BX+x]
	cmp	BX,DX
	jnb	short IGNORE

	mov	AX,CX	; CX = seg
	imul	graph_VramWidth
	mov	CX,BX
	and	CX,7
	shr	BX,1
	shr	BX,1
	shr	BX,1
	add	BX,AX
	mov	ES,graph_VramSeg

	mov	DX,VGA_PORT

	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX

	mov	CH,80h		; CH = 0x80 >> (x % 8)
	shr	CH,CL
	mov	CL,0

	mov	AX,VGA_READPLANE_REG or (3 shl 8)
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	dec	AH
	mov	AL,VGA_READPLANE_REG
	out	DX,AX
	mov	AL,CH
	and	AL,ES:[BX]
	add	AL,0ffh
	rcl	CL,1

	mov	AL,CL	; AH=0

IGNORE:
	ret	4
endfunc		; }

END
