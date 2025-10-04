; master library - PC-9801V GRCG supersfx
;
; Description:
;	
;
; Functions/Procedures:
;	void super_zoom_v_put_1plane(int x,int y,int num,unsigned rate,
;					int pattern_plane,unsigned put_plane);
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
;	94/ 7/19 Initial: superzv1.asm/master.lib 0.23 from supersfx.lib(iR)

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:word, super_patdata:word

	.CODE

func SUPER_ZOOM_V_PUT_1PLANE	; super_zoom_v_put_1plane() {
	enter	4,0
	push	DS
	push	SI
	push	DI

	paramsize = 6*2
	org_x		equ word ptr [BP+(RETSIZE+6)*2]
	org_y		equ word ptr [BP+(RETSIZE+5)*2]
	num		equ word ptr [BP+(RETSIZE+4)*2]
	rate		equ word ptr [BP+(RETSIZE+3)*2]
	pat_plane	equ word ptr [BP+(RETSIZE+2)*2]
	put_plane	equ word ptr [BP+(RETSIZE+1)*2]

	x_bytes		equ word ptr [BP-2]
	x_bytes_l	equ byte ptr [BP-2]
	x_bytes_h	equ byte ptr [BP-1]

	y_dots		equ byte ptr [BP-4]

	; パターンサイズ、アドレス
	mov	BX,num
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	x_bytes_h,0
	mov	x_bytes_l,CH	; x方向のバイト数
	mov	y_dots,CL	; y方向のドット数

	mov	AL,CH
	mul	CL		; AX = 1プレーン分のパターンバイト数
	xor	SI,SI		; pattern address offset
	mov	CX,pat_plane
	jcxz	short _4
_3:	add	SI,AX
	loop	_3
_4:

	imul	DI,org_y,80
	mov	AX,org_x
	mov	CX,AX
	shr	AX,3
	add	DI,AX		; GVRAM address offset
	and	CL,07h		; shift bit count

	; put開始
	mov	DS,super_patdata[BX]
				; pattern address segment
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
return:
	pop	DI
	pop	SI
	pop	DS
	leave
	ret	paramsize
endfunc		; }

disp		proc	near
	push	DI

	mov	BX,128		; y倍率カウンタ
	mov	DH,y_dots
	even
for_y:
	add	BX,rate
	test	BH,BH		; y縮小のときに消えるラインの
	jz	short next_y	; 処理をスキップ
	even
for_line:
	push	SI
	push	DI
	xor	AX,AX
	mov	CH,x_bytes_l
	even
for_x:
	lodsb
	mov	DL,AL
	shr	AX,CL
	stosb
	mov	AH,DL
	dec	CH
	jnz	for_x
	xor	AL,AL
	shr	AX,CL
	stosb
	pop	DI
	pop	SI
	add	DI,80
	dec	BH
	jnz	for_line
next_y:
	add	SI,x_bytes
	dec	DH
	jnz	for_y

	pop	DI
	ret
disp		endp

END
