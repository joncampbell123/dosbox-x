; master library - PC-9801V GRCG supersfx
;
; Description:
;	
;
; Functions/Procedures:
;	void super_wave_put(int x, int y, int num, int len, char amp, int ph);
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
;	94/ 7/19 Initial: superwav.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:word, super_patdata:word
	extrn	EDGES:word
	extrn	SinTable7:byte

	.DATA?

wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
count		db	?

	.CODE

func SUPER_WAVE_PUT	; super_wave_put() {
	enter	2,0
	push	DS
	push	SI
	push	DI
	push	BP

	; 引数
	x	= (RETSIZE+6)*2
	y	= (RETSIZE+5)*2
	num	= (RETSIZE+4)*2
	len	= (RETSIZE+3)*2
	amp	= (RETSIZE+2)*2
	ph	= (RETSIZE+1)*2

	flipflop = -2

	; パターンサイズ、アドレス
	mov	bx,[BP+num]
	add	bx,bx		; integer size & near pointer
	mov	cx,super_patsize[bx]	; pattern size (1-8)
	inc	ch
	mov	[count],ch
	mov	byte ptr cs:[ydots],cl
	push	super_patdata[bx]	; pattern address segment

	imul	di,word ptr [BP+y],80

	mov	ax,0ffffh
	and	ax,[BP+len]
	mov	[BP+flipflop],ah
	jns	short _1
	neg	word ptr [BP+len]
_1:
	xor	ch,ch
	xor	si,si		; i = 0
	even
for:
	; x座標の計算
	;	t = (256 * y / len + ph) % 256;
	;	x = x0 + amp * sin(t);
	mov	ax,si
	cwd
	shl	ax,7		; dx:ax = i * 256
	div	word ptr [BP+len] ; ax = dx:ax / len
	add	ax,[BP+ph]	; ax += ph
				; al = ax % 256 は当然省略
	mov	bx,offset SinTable7
	xlat			; al = sin(al) * 127
	imul	byte ptr [BP+amp] ; ax = al * amp
	sar	ax,7		; 本当は127で割るところを手抜き
	add	ax,[BP+x]	; ax += x
; GVRAMアドレスとシフトカウンタを計算
	mov	dx,ax
	and	al,00001111b	; ax = x % 16 (shift dot counter)
	xor	dl,al
	sar	dx,3		; dx = x / 8
	add	dx,di		; GVRAM offset address
	mov	ah,[count]
; xdotsが8の奇数倍のときの補正
	test	ah,00000001b
	jnz	short set_table
	xor	al,00001000b
; テーブルに設定
set_table:
	mov	wave_address[si],dx
	mov	wave_shift[si],ax
	xor	bh,bh
	mov	bl,al
	add	bx,bx
	mov	dx,EDGES[bx]
	mov	wave_mask[si],dx

	test	byte ptr [BP+flipflop],0ffh
	jz	short _2
	neg	byte ptr [BP+amp]
_2:
	add	di,80
	add	si,2
	loop	for

	pop	ds		; pattern address segment
	xor	si,si		; pattern address offset

; put開始
	mov	ax,0a800h
	mov	es,ax
	cld
; クリア
	mov	al,11000000b
	out	7ch,al		; RMW mode
	xor	al,al
	out	7eh,al
	out	7eh,al
	out	7eh,al
	out	7eh,al
	call	disp
; Bプレーン
	mov	al,11001110b
	out	7ch,al		; RMW mode
	mov	al,0ffh
	out	7eh,al
	out	7eh,al
	out	7eh,al
	out	7eh,al
	call	disp
; Rプレーン
	mov	al,11001101b
	out	7ch,al		; RMW mode
	call	disp
; Gプレーン
	mov	al,11001011b
	out	7ch,al		; RMW mode
	call	disp
; Iプレーン
	mov	al,11000111b
	out	7ch,al		; RMW mode
	call	disp

	xor	al,al
	out	7ch,al		; grcg stop
return:
	pop	BP
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	12
endfunc			; }

disp		proc	near
	mov	ss:[count],0	; dummy
ydots	equ	$-1
	xor	bp,bp		; できればbpは使いたくなかった
	even
put_loop:
	xor	dx,dx
	mov	di,wave_address[bp]
	mov	cx,wave_shift[bp]
	mov	bx,wave_mask[bp]
	shr	ch,1
	jc	short put_loop1
	lodsb
	shl	ax,8		; mov ah,al & mov al,0
	test	cl,00001000b
	jz	short odd1
	ror	ax,cl
	jmp	short odd2
	even
put_loop1:
	lodsw
odd1:	ror	ax,cl
	xor	dx,ax
	and	ax,bx
	xor	ax,dx
	stosw
odd2:	xor	dx,ax
	dec	ch
	jnz	put_loop1
	mov	es:[di],dx
	add	bp,2
	dec	ss:[count]
	jnz	put_loop
	ret
disp		endp

END
