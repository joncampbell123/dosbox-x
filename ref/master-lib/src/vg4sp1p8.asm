; superimpose & master library module
;
; Description:
;	パターンの表示(指定プレーンのみ, 横8dot単位)
;
; Functions/Procedures:
;	void vga4_super_put_1plane_8( int x, int y, int num, int pat_p, int put_p ) ;
;
; Parameters:
;	int x,y		表示座標
;	int num		パターン番号
;	int pat_p	パターンの中のプレーン番号
;	int put_p	画面上のプレーン番号
;			(上位8bit: 0=該当ドットをリセット  0ffh=セット)
;			(下位4bit: 1110b=青plane, 1101b=赤plane ... )
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA 16Color
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
;$Id: super1pl.asm 0.03 92/05/29 20:19:10 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] WORD_MASK廃止
;	94/ 6/ 7 Initial: vg4sp1pl.asm/master.lib 0.23
;	94/ 6/ 7 Initial: vg4sp1p8.asm/master.lib 0.23
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

func VGA4_SUPER_PUT_1PLANE_8	; vga4_super_put_1plane_8() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	num	= (RETSIZE+3)*2
	pat_plane = (RETSIZE+2)*2
	put_plane = (RETSIZE+1)*2

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX

	mov	SI,[BP+x]
	mov	DI,[BP+y]
	mov	CX,[BP+pat_plane]
	mov	BX,[BP+put_plane]
	mov	AX,BX
	mov	AL,VGA_SET_RESET_REG
	and	AH,0fh
	out	DX,AX
	mov	DX,SEQ_PORT
	mov	AH,BL
	mov	AL,SEQ_MAP_MASK_REG
	not	AH
	and	AH,0fh
	out	DX,AX

	mov	AX,graph_VramWidth
	mul	DI
	mov	DI,AX
	shr	SI,3
	add	DI,SI		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	mov	AL,DH
	xor	AH,AH
	mul	DL
	xor	SI,SI
	jcxz	short plane_end
plane_search:
	add	SI,AX
	loop	short plane_search
plane_end:
	mov	CH,0

	mov	ES,graph_VramSeg
	mov	AL,byte ptr graph_VramWidth
	mov	DS,super_patdata[BX]
	mov	BH,0
	mov	BL,AL
	sub	BL,DH

	shr	DH,1		;DX -> DH
	jc	short odd_size1

	EVEN
put_loop1_y:
	mov	CL,DH
put_loop1:
	lodsw
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	loop	short put_loop1
	add	DI,BX
	dec	DL
	jnz	short put_loop1_y
	jmp	short RETURN

	EVEN
odd_size1:
put_loop2_y:
	mov	CL,DH
	or	CL,CL
	jz	short skip2
	EVEN
put_loop2:
	lodsw
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	loop	short put_loop2
skip2:
	lodsb
	test	ES:[DI],AL
	stosb
	add	DI,BX
	dec	DL
	jnz	short put_loop2_y

RETURN:
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	10
endfunc				; }

END
