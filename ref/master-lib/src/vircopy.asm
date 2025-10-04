; superimpose & master library module
;
; Description:
;	仮想VRAMの確保と画面からの転送
;
; Functions/Procedures:
;	int virtual_copy( void ) ;
;	void virtual_vram_copy( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	virtual_copy:
;		InsufficientMemory	メモリが128KB確保できない
;		NoError			成功
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: 8086
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
;$Id: vircopy.asm 0.02 93/01/15 11:49:19 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16] セグメント変数を一つにした
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 4/ 1 [M0.22k] mem_AllocIDのID間違い。super->vvram
;	95/ 4/ 1 [M0.22k] virtual_vramwords対応
;

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN HMEM_ALLOC:CALLMODEL	; memheap.asm
	EXTRN HMEM_FREE:CALLMODEL	; memheap.asm

	.DATA
	EXTRN virtual_seg:WORD		; virtual.asm
	EXTRN virtual_vramwords:WORD	; virtual.asm
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
VRAMWORDS	equ (640/16)*400
PLANESEG	equ 80*400/16

func VIRTUAL_COPY		; virtual_copy() {
	mov	AX,virtual_seg
	or	AX,AX
	jnz	short copy

	mov	AX,PLANESEG*4
	push	AX
	mov	mem_AllocID,MEMID_vvram
	call	HMEM_ALLOC
	jc	short fault
	mov	virtual_seg,AX
	mov	virtual_vramwords,VRAMWORDS

copy:
	call	CALLMODEL PTR VIRTUAL_VRAM_COPY
	clc
	mov	AX,NoError
	ret

fault:
	mov	AX,InsufficientMemory
	ret
endfunc				; }

	; 仮想VRAMへVRAMの内容を転写
func VIRTUAL_VRAM_COPY		; virtual_vram_copy() {
	push	DS
	push	SI
	push	DI

	xor	DI,DI
	mov	SI,DI

	mov	BX,virtual_seg
	mov	ES,BX

	mov	AX,0a800h
	mov	DS,AX
	mov	CX,VRAMWORDS
	rep	movsw

	xor	SI,SI
	mov	AX,0b000h
	mov	DS,AX
	mov	CX,VRAMWORDS
	rep	movsw

	xor	DI,DI
	add	BX,PLANESEG*2
	mov	ES,BX

	xor	SI,SI
	mov	AX,0b800h
	mov	DS,AX
	mov	CX,VRAMWORDS
	rep	movsw
	xor	DI,DI

	add	BX,PLANESEG
	mov	ES,BX

	xor	SI,SI
	mov	AX,0e000h
	mov	DS,AX
	mov	CX,VRAMWORDS
	rep	movsw

	pop	DI
	pop	SI
	pop	DS
	ret
endfunc				; }

END
