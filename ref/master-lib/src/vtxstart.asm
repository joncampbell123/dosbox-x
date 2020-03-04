; master library - PC/AT - graph - text
;
; Description:
;	DOS/V emulation text のエミュレーション
;	・グラフィックモードにもDOS/V emulation textの機能を付加する
;
; Functions/Procedures:
;	void vtextx_start(void)
;	void vtextx_end(void)
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
;	PC/AT
;
; Requiring Resources:
;	CPU: 186
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
;	94/10/25 Initial: vtxstart.asm/master.lib 0.23
;	94/ 1/ 3 [M0.23] graph_VramZoomに対応(200line modeで使えるように)
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc
	include super.inc

	EXTRN DOS_SETVECT:CALLMODEL
	EXTRN HMEM_ALLOCBYTE:CALLMODEL
	EXTRN HMEM_FREE:CALLMODEL
	EXTRN VTEXT_START:CALLMODEL
	EXTRN FONT_AT_READ:CALLMODEL

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN graph_VramWidth:WORD
	EXTRN graph_VramZoom:WORD
	EXTRN TextVramAdr:DWORD
	EXTRN TextVramWidth:WORD
	EXTRN TextVramSize:WORD
	EXTRN VTextState:WORD
	EXTRN vtext_colortable:BYTE
	EXTRN mem_AllocID:WORD		; mem.asm

	EXTRN vtextx_Seg:WORD
	EXTRN vtextx_Size:WORD

	.DATA?
fontbuf	db	32 dup (?)

	.CODE

INT_VECTOR equ 10h
bioshook	dd	0

NEWBIOS proc far
	cmp	AH,0FEh
	je	short GETVRAM	; 0feh
	ja	short DRAW	; 0ffh
	cmp	AH,0
	jne	short CHAIN
UNHOOK:
	push	ES
	push	AX
	xor	AX,AX
	mov	ES,AX
	mov	AX,word ptr CS:bioshook
	mov	word ptr ES:[INT_VECTOR*4],AX
	mov	AX,word ptr CS:bioshook+2
	mov	word ptr ES:[INT_VECTOR*4+2],AX
	mov	AX,seg DGROUP
	mov	ES,AX
	and	ES:VTextState, not 4000h
	pop	AX
	pop	ES
CHAIN:	jmp	dword ptr CS:bioshook
	EVEN

GETVRAM:
	mov	DI,seg DGROUP
	mov	ES,DI
	les	DI,ES:TextVramAdr
	iret
	EVEN

DRAW:
	push	DS
	pusha
	CLD
	STI
	mov	DX,seg DGROUP
	mov	DS,DX
	; in: ES:DI = text vram address
	;     CX    = length

	mov	AL,4
	sub	AL,byte ptr graph_VramZoom
	mov	CS:SHL_VAL,AL

	; clipping
	mov	AX,ES
	cmp	word ptr TextVramAdr+2,AX
	jne	OUT_OF_BUF
	mov	SI,DI		; ES:SI = text vram offset
	mov	AX,TextVramSize
	shr	DI,1
	sub	AX,DI
	jbe	OUT_OF_BUF
	cmp	CX,AX
	jb	short SIZEOK
	mov	CX,AX
SIZEOK:
	mov	BP,CX		; BP = length

	mov	DX,SEQ_PORT
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	out	DX,AX

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX

DRAWLOOP:
	mov	AX,SI
	shr	AX,1
	xor	DX,DX
	div	TextVramWidth	; DX=x, AX=y
	mov	DI,DX
	shl	AX,4
	org	$-1
SHL_VAL	db	?
	mul	graph_VramWidth
	add	DI,AX		; DI = x+y*16*graph_VramWidth

	mov	AX,ES:[SI]
	add	SI,2
	mov	BL,AH
	and	BX,0fh
	mov	BL,vtext_colortable[BX]		; 文字色の変換
	xchg	BL,AH
	shr	BL,4
	mov	BL,vtext_colortable[BX]		; 背景色の変換
	shl	BL,4
	or	BL,AH

	mov	AH,0
	test	AL,0e0h
	jns	short ANK	; 00～7f = ANK
	jp	short ANK	; 80～9f, e0～ff = 漢字
				; (ほんとは 81～9f, e0～fdね)
	mov	AH,AL
	mov	AL,ES:[SI]
	add	SI,2
	dec	BP
ANK:
	mov	CX,AX

	push	ES
	push	SI

	mov	ES,graph_VramSeg
	mov	AL,VGA_SET_RESET_REG
	mov	DX,VGA_PORT
	mov	AH,BL			; background color
	shr	AH,4
	out	DX,AX
	mov	byte ptr ES:[DI],0ffh	; write background
	test	ES:[DI],AL		; load latch
	mov	AH,BL			; foreground color
	and	AH,0fh
	out	DX,AX

	; in: ES:DI=graphic vram offset
	;     CX=character code

	mov	AX,0810h	; 8x16
	test	CH,CH
	jz	short A
	mov	AH,16		; 16x16
A:
	push	CX
	push	AX
	shr	AH,3
	mov	SI,AX			; csize backup to SI
	push	DS
	push	offset fontbuf
	call	FONT_AT_READ

	mov	BX,SI			; BX = csize
	mov	CL,BL			; CX = ylen
	mov	CH,0
	mov	BL,BH
	mov	DX,graph_VramWidth
	mov	BH,0
	sub	DX,BX			; DX(xadd) = graph_VramWidth-xlen

	mov	ES,graph_VramSeg
	; CX = ylen
	; ES:DI = vram address
	mov	SI,offset fontbuf
	shr	CX,1

	cmp	byte ptr graph_VramZoom,0
	jne	short CHALF

	; 文字の描画
	cmp	BL,1		; xlen
	je	short HANKAKU
ZANKAKU: movsw
	add	DI,DX
	movsw
	add	DI,DX
	loop	short ZANKAKU
	jmp	short CDONE
	EVEN
HANKAKU: movsb
	add	DI,DX
	movsb
	add	DI,DX
	loop	short HANKAKU
	jmp	short CDONE

CHALF:
	; 文字の描画
	cmp	BL,1		; xlen
	je	short HANKAKU2
ZANKAKU2: lodsw
	or	AX,[SI]
	stosw
	add	SI,2
	add	DI,DX
	loop	short ZANKAKU2
	jmp	short CDONE
	EVEN
HANKAKU2: lodsw
	or	AL,AH
	stosb
	add	DI,DX
	loop	short HANKAKU2

CDONE:
	pop	SI
	pop	ES

	dec	BP
	jg	DRAWLOOP		; faraway
	jmp	short OUT_OF_BUF

OUT_OF_BUF:
	popa
	pop	DS
	iret
NEWBIOS endp

func VTEXTX_START	; vtextx_start() {
	CLD
	test	VTextState,8000h
	jz	short SKIP			; text mode -> skip

	mov	AX,TextVramSize
	cmp	vtextx_Size,AX
	mov	ES,vtextx_Seg
	je	short SKIP_ALLOC

	mov	vtextx_Size,AX
	add	AX,AX
	push	AX		; HMEM_ALLOCBYTEの引き数

	push	ES
	call	HMEM_FREE	; 古いブロックの開放(0のときは無視される)

	mov	mem_AllocID,MEMID_vtx
	call	HMEM_ALLOCBYTE
	jc	short SKIP
	mov	vtextx_Seg,AX
	push	DI
	mov	ES,AX
	mov	CX,TextVramSize
	mov	DI,0
	mov	AX,0720h	; 空白・白で塗り潰す
	rep	stosw
	pop	DI

SKIP_ALLOC:
	mov	word ptr TextVramAdr+2,ES
	mov	AX,0
	mov	word ptr TextVramAdr,AX
	mov	ES,AX
	mov	AX,TextVramWidth
	mov	ES:[44ah],AL			; 桁数
	mov	AX,TextVramSize
	div	byte ptr TextVramWidth
	dec	AL
	mov	ES:[484h],AL			; 行数

	test	VTextState,4000h		; text graphic mode
	jnz	short SKIP

	push	INT_VECTOR
	push	CS
	push	offset NEWBIOS
	call	DOS_SETVECT
	mov	word ptr CS:bioshook,AX
	mov	word ptr CS:bioshook+2,DX
	or	VTextState,4000h		; text graphic mode
SKIP:
	call	VTEXT_START
	ret
endfunc		; }

func VTEXTX_END		; vtextx_end() {
	mov	AX,word ptr CS:bioshook+2
	test	AX,AX
	jz	short END_SKIP1
	and	VTextState, not 4000h
	push	INT_VECTOR
	push	AX
	push	word ptr CS:bioshook
	call	DOS_SETVECT
	xor	AX,AX
	mov	word ptr bioshook+2,AX
	mov	word ptr bioshook,AX
END_SKIP1:
	push	vtextx_Seg
	call	HMEM_FREE
	xor	AX,AX
	mov	vtextx_Seg,AX
	mov	vtextx_Size,AX
	ret
endfunc		; }

END
