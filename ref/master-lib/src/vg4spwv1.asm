; master library - VGA 16Color
;
; Description:
;	
;
; Functions/Procedures:
;	void super_wave_put_1plane(int x,int y,int num,
;					int len,char amp,int ph,
;					int pattern_plane,unsigned put_plane);
;	
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
;	SS=DS前提 (BP相対でDGROUP dataをアクセスしている)
;	横640dot固定
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
;	94/ 7/17 Initial: vg4wave1.asm/master.lib 0.23

	.186
	.model small
	include	func.inc
	include	vgc.inc

	extrn	super_patsize:word,super_patdata:word
	extrn	EDGES:word
	extrn	SinTable7:byte

	.data?
	extrn	graph_VramSeg:WORD
	extrn	graph_VramWidth:WORD

wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
count		db	?

	.code

func VGA4_SUPER_WAVE_PUT_1PLANE	; vga4_super_wave_put_1plane() {
	enter	2,0
	push	DS
	push	SI
	push	DI

	paramsize = 8*2
	x		equ word ptr [BP+(RETSIZE+8)*2]
	y		equ word ptr [BP+(RETSIZE+7)*2]
	num		equ word ptr [BP+(RETSIZE+6)*2]
	len		equ word ptr [BP+(RETSIZE+5)*2]
	amp		equ byte ptr [BP+(RETSIZE+4)*2]
	ph		equ word ptr [BP+(RETSIZE+3)*2]
	pat_plane	equ word ptr [BP+(RETSIZE+2)*2]
	put_plane	equ word ptr [BP+(RETSIZE+1)*2]

	flipflop	equ byte ptr [BP-1]

	mov	DX,VGA_PORT
	mov	AX,VGA_MODE_REG or (VGA_CHAR shl 8)
	out	DX,AX
	mov	AX,VGA_BITMASK_REG or (0ffh shl 8)
	out	DX,AX
	mov	AX,VGA_DATA_ROT_REG or (VGA_PSET shl 8)
	out	DX,AX

	mov	BX,put_plane
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


	; パターンサイズ、アドレス
	mov	BX,num
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	AL,CH
	mul	CL
	push	AX		; 1パターンのバイト数
	inc	CH
	mov	[count],CH
	mov	byte ptr CS:[ydots],CL
	push	super_patdata[BX]	; pattern address segment

	mov	AX,graph_VramWidth
	imul	y
	mov	DI,AX

	mov	AX,0ffffh
	and	AX,len
	mov	[flipflop],AH
	jns	short _1
	neg	len
_1:
	xor	CH,CH
	xor	SI,SI		; i = 0
	even
for:
	; x座標の計算
	;	t = (256 * y / len + ph) % 256;
	;	x = x0 + amp * sin(t);
	mov	AX,SI
	cwd
	shl	AX,7		; DX:AX = i * 256
	div	len		; AX = DX:AX / len
	add	AX,ph		; AX += ph
				; AL = AX % 256 は当然省略
	mov	BX,offset SinTable7
	xlat			; AL = sin(AL) * 127
	imul	amp		; AX = AL * amp
	sar	AX,7		; 本当は127で割るところを手抜き
	add	AX,x		; AX += x

	; GVRAMアドレスとシフトカウンタを計算
	mov	DX,AX
	and	AL,00001111b	; AX = x % 16 (shift dot counter)
	xor	DL,AL
	sar	DX,3		; DX = x / 8
	add	DX,DI		; GVRAM offset address
	mov	AH,[count]

	; xdotsが8の奇数倍のときの補正
	test	AH,00000001b
	jnz	short set_table
	xor	AL,00001000b
; テーブルに設定
set_table:
	mov	wave_address[SI],DX
	mov	wave_shift[SI],AX
	xor	BH,BH
	mov	BL,AL
	add	BX,BX
	mov	DX,EDGES[BX]
	mov	wave_mask[SI],DX

	test	[flipflop],0ffh
	jz	short _2
	neg	amp
_2:
	add	DI,80		; 横640dot固定ですな
	add	SI,2
	loop	for

	mov	ES,graph_VramSeg
	pop	DS		; pattern address segment
	xor	SI,SI		; pattern address offset
	pop	AX		; 1パターンのバイト数
	mov	CX,pat_plane
	jcxz	short _4
_3:	add	SI,AX
	loop	_3
_4:

; put開始
	CLD

	call	disp

return:
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	paramsize
endfunc			; }

disp		proc	near
	push	BP
	mov	SS:[count],0	; dummy
ydots	equ	$-1
	xor	BP,BP		; できればBPは使いたくなかった
	even
put_loop:
	xor	DX,DX
	mov	DI,wave_address[BP]
	mov	CX,wave_shift[BP]
	mov	BX,wave_mask[BP]
	shr	CH,1
	jc	short put_loop1
	lodsb
	shl	AX,8		; mov AH,AL & mov AL,0
	test	CL,00001000b
	jz	short odd1
	ror	AX,CL
	jmp	short odd2
	even
put_loop1:
	lodsw
odd1:	ror	AX,CL
	xor	DX,AX
	and	AX,BX
	xor	AX,DX

	test	ES:[DI],AL
	mov	ES:[DI],AL
	test	ES:[DI+1],AH
	mov	ES:[DI+1],AH
	add	DI,2

odd2:	xor	DX,AX
	dec	CH
	jnz	put_loop1

	test	ES:[DI],DL
	mov	ES:[DI],DL
	test	ES:[DI+1],DH
	mov	ES:[DI+1],DH

	add	BP,2
	dec	SS:[count]
	jnz	put_loop
	pop	BP
	ret
disp		endp

END
