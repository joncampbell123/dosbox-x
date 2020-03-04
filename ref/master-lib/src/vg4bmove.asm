; master library - 
;
; Description:
;	VGA16色, 横8dot単位の領域移動
;
; Function/Procedures:
;	procedure vga4_byte_move( x1,y1,x2,y2,tox,toy: word ) ;
;
; Parameters:
;	
;
; Returns:
;	
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
;	クリッピングしてません。領域が重なる時は変になるでせう。
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
;	93/12/25 Initial: vg4bmove.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA

	EXTRN	graph_VramWidth:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

func VGA4_BYTE_MOVE	; vga4_byte_move() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	;
	x1	= (RETSIZE+6)*2
	y1	= (RETSIZE+5)*2
	x2	= (RETSIZE+4)*2
	y2	= (RETSIZE+3)*2
	tox	= (RETSIZE+2)*2
	toy	= (RETSIZE+1)*2

	mov	BX,graph_VramWidth
	mov	ES,graph_VramSeg

	mov	AX,[BP+y1]
	mul	BX
	mov	SI,[BP+x1]
	add	SI,AX

	mov	AX,[BP+toy]
	mul	BX
	mov	DI,[BP+tox]
	add	DI,AX

	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	out	DX,AX
	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_LATCH shl 8)
	out	DX,AX

	mov	DX,[BP+x2]
	sub	DX,[BP+x1]
	inc	DX
	sub	BX,DX
	mov	CX,[BP+y2]
	sub	CX,[BP+y1]
	inc	CX
	CLD
	mov	BP,CX

	EVEN
YLOOP:	mov	CX,DX
	rep	movs byte ptr ES:[DI],ES:[SI]
	add	SI,BX
	add	DI,BX
	dec	BP
	jnz	short YLOOP

	pop	DI
	pop	SI
	pop	BP
	ret	6*2
endfunc		; }

END
