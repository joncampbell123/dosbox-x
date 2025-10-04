; master.lib module
;
; Description:
;	仮想VRAMから実画面へ転送
;
; Functions/Procedures:
;	void vga4_virtual_repair( int x, int y, int num ) ;
;
; Parameters:
;	x,y 転送範囲を示すパターンの座標
;	num 転送範囲を示すパターンの番号
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
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: virrep.asm 0.06 92/05/29 20:44:06 Kazumi Rel $
;                     91/09/17 01:10:10 Mikabon(ﾐ) Bug Fix??
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]
;	94/10/21 Initial: vg4vrep.asm / master.lib 0.23

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc
	.DATA

	EXTRN	graph_VramWidth:WORD
	EXTRN	graph_VramWords:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	super_patsize:WORD
	EXTRN	virtual_seg:WORD

	.CODE

func VGA4_VIRTUAL_REPAIR	; vga4_virtual_repair() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	AX,graph_VramWords
	shr	AX,3
	mov	CS:VIRTUALPARA,AX

	CLD
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	AX,[BP+y]
	mov	BP,[BP+x]

	mov	ES,graph_VramSeg
	mov	DI,graph_VramWidth
	mul	DI
	mov	CX,BP
	shr	BP,3		;BP=x/8
	add	BP,AX		;GVRAM offset address

	and	CX,7
	neg	CX
	mov	CX,super_patsize[BX]		;pattern size
	adc	CH,0

	mov	BX,DI
	sub	BL,CH

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_NORMAL shl 8)
	out	DX,AX
;	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
;	out	DX,AX
;	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
;	out	DX,AX
	mov	AX,VGA_ENABLE_SR_REG or (0 shl 8)
	out	DX,AX

	; B
	mov	DX,virtual_seg
	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	DISP

	; R
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	call	DISP

	; G
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	DISP

	; I
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	DISP

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

; in: AX (SEQ_PORTに出す値)
;     BP 描画開始アドレス
;     CX 1lineごとのバイト数
;     BX 1lineごとの加算値
;     DX 元データのセグメント値
; out:
;     DX 次のセグメント値
; break: AX,SI,DI,DS
; save: BX,CX,BP
DISP	proc near
	mov	DS,DX
	mov	DX,SEQ_PORT
	out	DX,AX
	mov	AX,CX
	mov	DH,AL
	mov	DI,BP
	mov	CH,0
	EVEN
PLOOP:
	mov	SI,DI
	mov	CL,AH
	rep	movsb
	add	DI,BX
	dec	DH
	jnz	short PLOOP
	mov	CX,AX
	mov	DX,DS
	add	DX,1234h	; dummy
	org $-2
VIRTUALPARA dw ?
	ret
DISP	endp

END
