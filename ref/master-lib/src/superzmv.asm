; master library - PC-9801V GRCG supersfx
;
; Description:
;	
;
; Functions/Procedures:
;	void super_zoomv_put(int x, int y, int num, unsigned rate);
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
;	94/ 7/19 Initial: superzmv.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:word, super_patdata:word

wd_bt	struc
	l	db	?
	h	db	?
wd_bt	ends

	.CODE

func SUPER_ZOOM_V_PUT	; super_zoom_v_put() {
	enter	4,0
	push	DS
	push	SI
	push	DI

	org_x	= (RETSIZE+4)*2
	org_y	= (RETSIZE+3)*2
	num	= (RETSIZE+2)*2
	rate	= (RETSIZE+1)*2

	x_bytes	= -2
	y_dots  = -3
	y_count = -4

	; パターンサイズ、アドレス
	mov	bx,[BP+num]
	add	bx,bx		; integer size & near pointer
	mov	cx,super_patsize[bx]	; pattern size (1-8)
	mov	[BP+x_bytes].h,0
	mov	[BP+x_bytes].l,ch	; x方向のバイト数
	mov	[BP+y_dots],cl	; y方向のドット数
;	mov	al,ch
;	mul	cl
;	mov	[pat_bytes],ax	; 1プレーン分のパターンバイト数

	imul	di,word ptr [BP+org_y],80
	mov	ax,[BP+org_x]
	mov	cx,ax
	shr	ax,3
	add	di,ax		; GVRAM address offset
	and	cl,07h		; shift bit count

	mov	ds,super_patdata[bx]
				; pattern address segment
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
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	8
endfunc		; }

disp	proc	near
	push	DI

	mov	bx,128		; y倍率カウンタ
	mov	dh,[BP+y_dots]
	even
for_y:
	add	bx,[BP+rate]
	test	bh,bh		; y縮小のときに消えるラインの
	jz	short next_y	; 処理をスキップ
	even
for_line:
	push	si
	push	di
	xor	ax,ax
	mov	ch,[BP+x_bytes].l
	even
for_x:
	lodsb
	mov	dl,al
	shr	ax,cl
	stosb
	mov	ah,dl
	dec	ch
	jnz	for_x
	xor	al,al
	shr	ax,cl
	stosb
	pop	di
	pop	si
	add	di,80
	dec	bh
	jnz	for_line
next_y:
	add	si,[BP+x_bytes]
	dec	dh
	jnz	for_y

	pop	DI
	ret
disp		endp

END
