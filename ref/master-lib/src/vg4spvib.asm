; master library - VGA 16Color
;
; Description:
;	
;
; Functions/Procedures:
;	void vga4_super_vibra_put(int x, int y, int num, int len, int ph);
;
; Parameters:
;	
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	VGA
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	SS=DSが前提!!!!
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	iR
;	恋塚昭彦
;
; Revision History:
;	94/ 7/17 Initial: vg4vibra.asm/master.lib 0.23
;
		.186
		.model small
		include	func.inc
		include	vgc.inc

		extrn	super_patsize:word, super_patdata:word
		extrn	EDGES:word
		extrn	SinTable7:byte

		.data?
		extrn	graph_VramSeg:WORD
		extrn	graph_VramWidth:WORD

line_address	dw	64 dup (?)
wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
pat_offset	dw	?
nbytes		dw	?
count		db	?

		.code

func VGA4_SUPER_VIBRA_PUT	; {
	enter	4,0
	push	BP
	push	SI
	push	DI
	push	DS

	paramsize = 5*2
	x		equ word ptr [BP+(RETSIZE+5)*2]
	y		equ word ptr [BP+(RETSIZE+4)*2]
	num		equ word ptr [BP+(RETSIZE+3)*2]
	len		equ word ptr [BP+(RETSIZE+2)*2]
	ph		equ word ptr [BP+(RETSIZE+1)*2]

	lines		equ byte ptr [BP-2]
	foo		equ word ptr [BP-4]

	; パターンサイズ、アドレス
	mov	bx, num
	add	bx, bx		; integer size & near pointer
	mov	cx, super_patsize[bx]	; pattern size (1-8)
	mov	al, ch
	mul	cl
	mov	[nbytes], ax	; 1パターンのバイト数
	mov	[lines], ch
	inc	ch
	mov	[count], ch
	mov	byte ptr cs:[ydots], cl
	push	super_patdata[bx]	; pattern address segment

	mov	AX,graph_VramWidth
	imul	y
	mov	DI,AX
	mov	ES, graph_VramSeg

	xor	ch, ch
	xor	si, si		; i = 0
	even
for:
	; y変位の計算
	;	t = (256 * y / len + ph) % 256;
	;	a = 128 * y / Y
	;	dy = (Y / 4) * sin(t) * sin(a);
	mov	ax, si
	cwd
	shl	ax, 7		; dx:ax = i * 256
	div	len		; ax = dx:ax / len
	add	ax, ph		; ax += ph
				; al = ax % 256 は当然省略
	mov	bx, offset SinTable7
	xlat			; al = sin(al) * 127
	imul	byte ptr cs:[ydots]	; ax = al * Y
	mov	[foo], ax		; w = ax

	mov	ax, si
	shl	ax, 6		; ax = i * 128
	div	byte ptr cs:[ydots]	; al = ax / Y
	xlat			; al = sin(al) * 127
	cbw			; ax = al

	imul	[foo]		; dx:ax = ax * w
	mov	ax, dx		; ax = dx:ax / (128 * 128 * 4)
	; GVRAMアドレスとシフトカウンタを計算
	sal	ax, 0
	add	ax, si
	sar	ax, 1
	imul	[lines]
	mov	line_address[si], ax

	mov	ax, x
	mov	dx, ax
	and	al, 00001111b	; ax = x % 16 (shift dot counter)
	xor	dl, al
	sar	dx, 3		; dx = x / 8
	add	dx, di		; GVRAM offset address
	mov	ah, [count]
	; xdotsが8の奇数倍のときの補正
	test	ah, 00000001b
	jnz	short set_table
	xor	al, 00001000b
	; テーブルに設定
set_table:
	mov	wave_address[si], dx
	mov	wave_shift[si], ax
	xor	bh, bh
	mov	bl, al
	add	bx, bx
	mov	dx, EDGES[bx]
	mov	wave_mask[si], dx

	add	di, 80
	add	si, 2
	loop	for

	xor	ax, ax
	mov	[pat_offset], ax; pattern address offset
	pop	ds		; pattern address segment

	; put開始
	CLD
	; クリア
	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_SET_RESET_REG or (0 shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX
	mov	AX,SEQ_MAP_MASK_REG or (0fh shl 8)
	call	DISP		;originally cls_loop

	; Bプレーン
	mov	DX,VGA_PORT
	mov	AX,VGA_SET_RESET_REG or (0fh shl 8)
	out	DX,AX

	mov	AX,SEQ_MAP_MASK_REG or (1 shl 8)
	call	DISP
	mov	AX,SEQ_MAP_MASK_REG or (2 shl 8)
	; Rプレーン
	call	DISP
	; Gプレーン
	mov	AX,SEQ_MAP_MASK_REG or (4 shl 8)
	call	DISP
	; Iプレーン
	mov	AX,SEQ_MAP_MASK_REG or (8 shl 8)
	call	DISP

	pop	DS
	pop	DI
	pop	SI
	pop	BP
	leave
	ret paramsize
endfunc				; }

DISP		proc	near
mov	DX,SEQ_PORT
out	DX,AX
	mov	ss:[count], 0	; dummy
ydots	equ	$-1
	xor	bp, bp		; できればbpは使いたくなかった
	even
put_loop:
	xor	dx, dx
	mov	si, line_address[bp]
	add	si, ss:[pat_offset]
	mov	di, wave_address[bp]
	mov	cx, wave_shift[bp]
	mov	bx, wave_mask[bp]
	shr	ch, 1
	jc	short put_loop1
	lodsb
	shl	ax, 8		; mov ah, al & mov al, 0
	test	cl, 00001000b
	jz	short odd1
	ror	ax, cl
	jmp	short odd2
	even
put_loop1:
	lodsw
odd1:	ror	ax, cl
	xor	dx, ax
	and	ax, bx
	xor	ax, dx

	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2
odd2:	xor	dx, ax
	dec	ch
	jnz	put_loop1

	test	ES:[DI],DL
	mov	ES:[DI],DL
	test	ES:[DI+1],DH
	mov	ES:[DI+1],DH

	add	bp, 2
	dec	ss:[count]
	jnz	put_loop

	mov	ax, ss:[nbytes]
	add	ss:[pat_offset], ax
	ret
DISP		endp

END
