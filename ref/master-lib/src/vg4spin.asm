; master library module - VGA16Color
;
; Description:
;	VGA 16Color, キャラクタを表示する
;
; Functions/Procedures:
;	void vga4_super_in(int x,int y,int cahnum) ;
;
; Parameters:
;	x,y	座標
;	cahnum	キャラクタ番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 186
;	VGA
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	94/11/ 6 Initial: vg4spin.asm / master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD
	EXTRN	super_charnum:WORD, super_chardata:WORD
	EXTRN	graph_VramWidth:WORD, graph_VramSeg:WORD

	.CODE
	EXTRN	VGA4_SUPER_PUT:CALLMODEL

func VGA4_SUPER_IN		; vga4_super_in() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	charnum	= (RETSIZE+1)*2

	CLD
	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX

	mov	BX,[BP+charnum]
	mov	AX,graph_VramWidth
	mov	CX,AX
	mul	word ptr [BP+y]
	mov	SI,[BP+x]
	shr	SI,3		; GVRAM offset address
	add	SI,AX
	mov	AX,CX		; AX = graph_VramWidth

	shl	BX,1
	mov	ES,super_chardata[BX]
	mov	BX,super_charnum[BX]

	push	[BP+x]			; VGA4_SUPER_PUT の引き数
	push	[BP+y]			; VGA4_SUPER_PUT の引き数
	push	BX			; VGA4_SUPER_PUT の引き数

	push	DS

	shl	BX,1
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	xor	DI,DI
	mov	BP,SI
	mov	DS,graph_VramSeg
	mov	CL,DH
	xor	CH,CH
	inc	CX
	inc	CX		;1 byte
	and	CX,not 1
	mov	BX,AX		; graph_VramWidth
	sub	BL,CL
	mov	AX,VGA_READPLANE_REG or (0 shl 8)
	call	BACKUP
	mov	AX,VGA_READPLANE_REG or (1 shl 8)
	call	BACKUP
	mov	AX,VGA_READPLANE_REG or (2 shl 8)
	call	BACKUP
	mov	AX,VGA_READPLANE_REG or (3 shl 8)
	call	BACKUP

	pop	DS

	call	VGA4_SUPER_PUT

	pop	DI
	pop	SI
	pop	BP
	ret	6
endfunc				; }


; DS:(SI=BP = vram
; ES:DI = backup data area
; CX = x bytes
; DL = ylen
BACKUP	proc	near
	mov	SI,DX
	mov	DX,VGA_PORT
	out	DX,AX
	mov	DX,SI

	mov	AX,CX
	mov	SI,BP

	mov	DH,DL
	shr	AX,1
	jnb	short REPAIR_EVEN

	EVEN
REPAIR_ODD:
	mov	CX,AX
	movsb
	rep	movsw
	lea	SI,[SI+BX]
	dec	DH
	jnz	short REPAIR_ODD
	rcl	AX,1
	mov	CX,AX
	ret

	EVEN
REPAIR_EVEN:
	mov	CX,AX
	rep	movsw
	add	SI,BX
	dec	DH
	jnz	short REPAIR_EVEN
	shl	AX,1
	mov	CX,AX
	ret
BACKUP	endp

END
