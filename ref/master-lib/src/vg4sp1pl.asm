; superimpose & master library module
;
; Description:
;	パターンの表示(指定プレーンのみ)
;
; Functions/Procedures:
;	void vga4_super_put_1plane( int x, int y, int num, int pat_p, int put_p ) ;
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
;

	.186
	.MODEL SMALL
	include func.inc
	include vgc.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	BYTE_MASK:BYTE
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramWidth:WORD

	.CODE

func VGA4_SUPER_PUT_1PLANE	; vga4_super_put_1plane() {
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

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	SI,[BP+pat_plane]
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
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	push	BX
	mov	AL,DH
	xor	AH,AH
	mul	DL
	mov	BP,AX
	pop	AX
	xor	BX,BX
	or	SI,SI
	jz	short plane_end
plane_search:
	add	BX,BP
	dec	SI
	jnz	plane_search
plane_end:
	mov	ES,BX
	mov	SI,CX
	mov	BL,BYTE_MASK[SI]
	mov	CH,DH		;DL -> DH
	shr	CH,1

	mov	BP,DI		;save DI
	test	DH,1		;DX -> DH
	jnz	short odd_size1
	mov	BYTE PTR CS:[word_mask1],BL
	mov	BYTE PTR CS:[count1],CH
	mov	BX,AX
	mov	SI,ES
	mov	ES,graph_VramSeg
	mov	AL,byte ptr graph_VramWidth
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	sub	AL,DH		;DL -> DH
	mov	BYTE PTR CS:[add_di1],AL
	xor	DL,DL
	EVEN
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask1	EQU	$-1
	xor	DH,AL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	put_loop1
	test	ES:[DI],DL
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di1		EQU	$-1
	mov	CH,11h	;dummy
count1		EQU	$-1
	dec	BX
	jnz	put_loop1
	jmp	short RETURN


	EVEN
odd_size1:
	mov	BYTE PTR CS:[word_mask2],BL
	mov	BYTE PTR CS:[count2],CH
	mov	BX,AX
	mov	SI,ES
	mov	ES,graph_VramSeg
	mov	AL,byte ptr graph_VramWidth
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	sub	AL,DH
	mov	BYTE PTR CS:[add_di2],AL
	xor	DL,DL
	EVEN
single_check2:
	or	CH,CH
	jz	short skip2
	EVEN
put_loop2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask2	EQU	$-1
	xor	DH,AL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
	mov	DL,DH
	dec	CH
	jnz	put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	inc	DI
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di2		EQU	$-1
	mov	CH,11h	;dummy
count2		EQU	$-1
	dec	BX
	jnz	short single_check2

RETURN:
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	10
endfunc				; }

END
