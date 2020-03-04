; master library - PC-9801V GRCG supersfx
;
; Description:
;	
;
; Functions/Procedures:
;	void super_wave_put_1plane(int x, int y, int num,
;					int len, char amp, int ph,
;					int pattern_plane, unsigned put_plane);
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
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
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
;	iR
;
; Revision History:
;	94/ 7/19 Initial: superwv1.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.186
	.MODEL SMALL
	include func.inc

	.DATA?
	extrn	super_patsize:word, super_patdata:word
	extrn	EDGES:word
	extrn	SinTable7:byte

wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
count		db	?

	.CODE

func SUPER_WAVE_PUT_1PLANE	; super_wave_put_1plane() {
	enter	2,0
	push	DS
	push	SI
	push	DI
	push	BP

	paramsize = 8*2
	x		equ word ptr [BP+(RETSIZE+8)*2]
	y		equ word ptr [BP+(RETSIZE+7)*2]
	num		equ word ptr [BP+(RETSIZE+6)*2]
	len		equ word ptr [BP+(RETSIZE+5)*2]
	amp		equ byte ptr [BP+(RETSIZE+4)*2]
	ph		equ word ptr [BP+(RETSIZE+3)*2]
	pat_plane	equ word ptr [BP+(RETSIZE+2)*2]
	put_plane	equ word ptr [BP+(RETSIZE+1)*2]

	flipflop	equ byte ptr [BP-2]

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

	imul	DI,y,80

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
	add	DI,80
	add	SI,2
	loop	for

	pop	DS		; pattern address segment
	xor	SI,SI		; pattern address offset
	pop	AX		; 1パターンのバイト数
	mov	CX,pat_plane
	jcxz	short _4
_3:	add	SI,AX
	loop	_3
_4:

	; put開始
	mov	AX,0a800h
	mov	ES,AX
	cld

	mov	AX,put_plane
	out	7ch,AL		; RMW mode
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	disp

	xor	AL,AL
	out	7ch,AL		; grcg stop

	pop	BP
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	paramsize
endfunc		; }

disp		proc	near
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
	stosw
odd2:	xor	DX,AX
	dec	CH
	jnz	put_loop1
	mov	ES:[DI],DX
	add	BP,2
	dec	SS:[count]
	jnz	put_loop
	ret
disp		endp

END
