; master library - PC-9801V GRCG supersfx
;
; Description:
;	
;
; Functions/Procedures:
;	void super_vibra_put_1plane(int x, int y, int num, int len, int ph,
;					int pattern_plane, unsinged put_plane);
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
;	94/ 7/19 Initial: supervb1.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:word, super_patdata:word
	extrn	EDGES:word
	extrn	SinTable7:byte

	.DATA?

line_address	dw	64 dup (?)
wave_address	dw	64 dup (?)
wave_shift	dw	64 dup (?)
wave_mask	dw	64 dup (?)
pat_offset	dw	?
nbytes		dw	?
count		db	?

	.CODE

func SUPER_VIBRA_PUT_1PLANE	; super_vibra_put_1plane() {
	enter	4,0
	push	DS
	push	SI
	push	DI
	push	BP

	paramsize = 7*2
	x	  equ word ptr [BP+(RETSIZE+7)*2]
	y	  equ word ptr [BP+(RETSIZE+6)*2]
	num	  equ word ptr [BP+(RETSIZE+5)*2]
	len	  equ word ptr [BP+(RETSIZE+4)*2]
	ph	  equ word ptr [BP+(RETSIZE+3)*2]
	pat_plane equ word ptr [BP+(RETSIZE+2)*2]
	put_plane equ word ptr [BP+(RETSIZE+1)*2]

	lines equ byte ptr [BP-2]
	foo   equ word ptr [BP-4]

	; �p�^�[���T�C�Y�A�A�h���X
	mov	bx,num
	add	bx,bx		; integer size & near pointer
	mov	cx,super_patsize[bx]	; pattern size (1-8)
	mov	al,ch
	mul	cl
	push	ax
	mov	[nbytes],ax	; 1�p�^�[���̃o�C�g��
	mov	[lines],ch
	inc	ch
	mov	[count],ch
	mov	byte ptr cs:[ydots],cl
	push	super_patdata[bx]	; pattern address segment

	imul	di,y,80

	xor	ch,ch
	xor	si,si		; i = 0
	even
for:
	; y�ψʂ̌v�Z
	;	t = (256 * y / len + ph) % 256;
	;	x = x0 + amp * sin(t);

	;	t = (256 * y / len + ph) % 256;
	;	a = 128 * y / Y
	;	dy = (Y / 4) * sin(t) * sin(a);
	mov	ax,si
	cwd
	shl	ax,7		; dx:ax = i * 256
	div	len		; ax = dx:ax / len
	add	ax,ph		; ax += ph
				; al = ax % 256 �͓��R�ȗ�
	mov	bx,offset SinTable7
	xlat			; al = sin(al) * 127
	imul	byte ptr cs:[ydots]	; ax = al * Y
	mov	[foo],ax		; w = ax

	mov	ax,si
	shl	ax,6		; ax = i * 128
	div	byte ptr cs:[ydots]	; al = ax / Y
	xlat			; al = sin(al) * 127
	cbw			; ax = al

	imul	[foo]		; dx:ax = ax * w
	mov	ax,dx		; ax = dx:ax / (128 * 128 * 4)
	; GVRAM�A�h���X�ƃV�t�g�J�E���^���v�Z
	sal	ax,0
	add	ax,si
	sar	ax,1
	imul	[lines]
	mov	line_address[si],ax

	mov	ax,x
	mov	dx,ax
	and	al,00001111b	; ax= x % 16 (shift dot counter)
	xor	dl,al
	sar	dx,3		; dx= x / 8
	add	dx,di		; GVRAM offset address
	mov	ah,[count]
	; xdots��8�̊�{�̂Ƃ��̕␳
	test	ah,00000001b
	jnz	short set_table
	xor	al,00001000b
	; �e�[�u���ɐݒ�
set_table:
	mov	wave_address[si],dx
	mov	wave_shift[si],ax
	xor	bh,bh
	mov	bl,al
	add	bx,bx
	mov	dx,EDGES[bx]
	mov	wave_mask[si],dx

	add	di,80
	add	si,2
	loop	for

	pop	ds		; pattern address segment
	xor	si,si		; pattern address offset
	pop	ax		; 1�p�^�[���̃o�C�g��
	mov	cx,pat_plane
	jcxz	short _4
_3:	add	si,ax
	loop	_3
_4:	mov	cs:[pat_offset],si; pattern address offset

	; put�J�n
	mov	ax,0a800h
	mov	es,ax
	cld

	mov	ax,put_plane
	out	7ch,al		; RMW mode
	mov	al,ah
	out	7eh,al
	out	7eh,al
	out	7eh,al
	out	7eh,al
	call	disp

	xor	al,al
	out	7ch,al		; grcg stop
return:
	pop	BP
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	paramsize
endfunc			; }

disp		proc	near
	mov	ss:[count],0	; dummy
ydots	equ	$-1
	xor	bp,bp		; �ł����bp�͎g�������Ȃ�����
	even
put_loop:
	xor	dx,dx
	mov	si,line_address[bp]
	add	si,ss:[pat_offset]
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

	mov	ax,ss:[nbytes]
	add	ss:[pat_offset],ax
	ret
disp		endp

END
