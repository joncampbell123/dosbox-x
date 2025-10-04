; master library - PC98
;
; Description:
;	グラフィック画面の任意の地点の色番号を得る
;
; Function/Procedures:
;	int graph_readdot( int x, int y ) ;
;
; Parameters:
;	int	x	ｘ座標(0～639)
;	int	y	ｙ座標(0～399)
;
; Returns:
;	int	色コード(0～15)または、ｘ座標が範囲外ならば -1
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
;	92/11/25 Initial
;	93/ 2/ 9 bugfix, clipping
;	93/ 3/ 9 clippingでまたバグってたくそーーー

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramLines:WORD

	.CODE

func GRAPH_READDOT	; {
	mov	BX,SP
	; 引数
	x	= (RETSIZE+1)*2
	y	= (RETSIZE+0)*2

	mov	AX,-1

	mov	DX,SS:[BX+y]
	cmp	DX,graph_VramLines
	jnb	short IGNORE

	mov	BX,SS:[BX+x]
	cmp	BX,640
	jnb	short IGNORE

	mov	AX,DX	; DX = seg
	shl	DX,1
	shl	DX,1
	add	DX,AX
	add	DX,graph_VramSeg
	add	DX,0e000h - 0a800h	; Iplane

	mov	CX,BX
	and	CX,7
	shr	BX,1
	shr	BX,1
	shr	BX,1		; BX = x / 8
	mov	AL,80h		; AL = 0x80 >> (x % 8)
	shr	AL,CL

	mov	ES,DX
	mov	AH,ES:[BX]
	and	AH,AL
	neg	AH
	rcl	CH,1
	sub	DX,0e000h - 0b800h	; I -> G plane

	mov	ES,DX
	mov	AH,ES:[BX]
	and	AH,AL
	neg	AH
	rcl	CH,1
	sub	DX,1000h		; G -> B plane

	mov	ES,DX
	mov	AH,ES:[BX+8000h]	; R plane
	and	AH,AL
	neg	AH
	rcl	CH,1

	and	AL,ES:[BX]		; B plane
	neg	AL
	rcl	CH,1

	mov	AL,CH
	mov	AH,0

IGNORE:
	ret	4
endfunc		; }

END
