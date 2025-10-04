; master library - 
;
; Description:
;	VGA 16color, ���z��ʂ̊m�ہA����ю���ʂ��牼�z��ʂւ̓]��
;
; Functions/Procedures:
;	int vga4_virtual_copy(void)
;	void vga4_virtual_vram_copy(void)
;
; Parameters:
;	none
;
; Returns:
;	virtual_copy:
;		InsufficientMemory	��������128KB�m�ۂł��Ȃ�
;		NoError			����
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;	VGA
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
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16] �Z�O�����g�ϐ�����ɂ���
;	94/10/21 Initial: vg4vcopy.asm/master.lib 0.23
;	95/ 2/14 [M0.22k] mem_AllocID�Ή�
;	95/ 4/ 1 [M0.22k] mem_AllocID��ID�ԈႢ�Bsuper->vvram
;	95/ 4/ 1 [M0.22k] virtual_vramwords�Ή�
;	95/ 4/12 [M0.22k] vga4_virtual_vram_copy �Ăяo�����_��cy=1���Ǝ���
;

	.MODEL SMALL
	include func.inc
	include vgc.inc
	include super.inc
	EXTRN HMEM_ALLOC:CALLMODEL	; memheap.asm
	EXTRN HMEM_FREE:CALLMODEL	; memheap.asm

	.DATA
	EXTRN graph_VramSeg:WORD	; grp.asm
	EXTRN graph_VramWords:WORD	; grp.asm
	EXTRN virtual_seg:WORD		; virtual.asm
	EXTRN virtual_vramwords:WORD	; virtual.asm
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE

func VGA4_VIRTUAL_COPY		; vga4_virtual_copy() {
	mov	AX,virtual_seg
	or	AX,AX
	jnz	short copy

	mov	AX,graph_VramWords	; word�� -> paragraph size
	mov	virtual_vramwords,AX
	add	AX,7
	rcr	AX,1
	and	AX,not 3
	push	AX
	mov	mem_AllocID,MEMID_vvram
	call	HMEM_ALLOC
	jc	short fault
	mov	virtual_seg,AX

copy:
	call	CALLMODEL PTR VGA4_VIRTUAL_VRAM_COPY
	clc
	mov	AX,NoError
	ret

fault:
	mov	AX,InsufficientMemory
	ret
endfunc				; }

	; ���zVRAM��VRAM�̓��e��]��
func VGA4_VIRTUAL_VRAM_COPY	; vga4_virtual_vram_copy() {
	push	BP
	push	DS
	push	SI
	push	DI

	CLD

	mov	BP,graph_VramWords
	mov	BX,BP
	add	BX,7		; convert word to paragraph
	rcr	BX,1
	shr	BX,1
	shr	BX,1

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_READPLANE shl 8)
	out	DX,AX

	mov	CX,virtual_seg
	mov	AX,VGA_READPLANE_REG or (0 shl 8)
	mov	DS,graph_VramSeg

PLANELOOP:
	mov	ES,CX

	out	DX,AX
	xor	DI,DI
	mov	SI,DI
	mov	CX,BP
	rep	movsw

	mov	CX,ES
	add	CX,BX

	inc	AH
	cmp	AH,4
	jl	short PLANELOOP

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret
endfunc				; }

END
