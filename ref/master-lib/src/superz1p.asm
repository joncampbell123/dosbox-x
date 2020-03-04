; master library - 
;
; Description:
;	パターンの拡大／縮小表示(任意プレーン)
;
; Function/Procedures:
;	void super_zoom_put_1plane(int x,int y,int num,
;				unsigned x_rate,unsigned y_rate,
;				int pattern_plane,unsigned put_plane);
;
; Parameters:
;	x,y	左上座標
;	num	パターン番号
;	x_rate	横の倍率(xrate/256倍)
;	y_rate	横の倍率(yrate/256倍)
;	pattern_plane	
;	put_plane	描画プレーン(super_put_1plane参照)
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
;	93/11/28 Initial: superz1p.asm/master.lib 0.22
;

	.186
	.MODEL	SMALL
	include func.inc

	.DATA
	extrn	super_patsize:word,super_patdata:word

	.CODE
	extrn	GRCG_BOXFILL:CALLMODEL
	extrn	GRCG_OFF:CALLMODEL


MRETURN macro
	pop	DI
	pop	SI

	mov	SP,BP

	pop	BP
	ret	7*2
	EVEN
	endm


func SUPER_ZOOM_PUT_1PLANE	; super_zoom_put_1plane() {
	push	BP
	mov	BP,SP
	sub	SP,12

	; 引数
	org_x	      equ WORD PTR [BP+(RETSIZE+7)*2]
	org_y	      equ WORD PTR [BP+(RETSIZE+6)*2]
	num	      equ WORD PTR [BP+(RETSIZE+5)*2]
	x_rate	      equ WORD PTR [BP+(RETSIZE+4)*2]
	y_rate	      equ WORD PTR [BP+(RETSIZE+3)*2]
	pattern_plane equ WORD PTR [BP+(RETSIZE+2)*2]
	put_plane     equ WORD PTR [BP+(RETSIZE+1)*2]

	; ローカル変数
	pat_bytes  equ WORD PTR [BP-2]
	y1_pos 	   equ WORD PTR [BP-4]
	y2_pos 	   equ WORD PTR [BP-6]
	x_len_256  equ WORD PTR [BP-8]
	x_len_256l equ BYTE PTR [BP-8]
	y_len_256  equ WORD PTR [BP-10]
	y_len_256l equ BYTE PTR [BP-10]

	plane_data equ BYTE PTR [BP-11]
	x_bytes	   equ BYTE PTR [BP-12]

	push	SI
	push	DI

; パターンサイズ、アドレス
	mov	BX,num
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	x_bytes,CH	; x方向のバイト数
				; CL には ydots
	mov	AL,CH
	mul	CL
	mov	pat_bytes,AX	; 1プレーン分のパターンバイト数
	mov	ES,super_patdata[BX]
				; パターンデータのセグメント
	push	CX
	xor	SI,SI		; SIはパターン内のオフセット
	mov	CX,pattern_plane
	jcxz	short _4
_3:	add	SI,AX
	loop	short _3
_4:	pop	CX

	mov	AX,put_plane
	out	7ch,AL		; RMW mode
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	y_len_256,128	; 偏りを無くす
	mov	AX,org_y
	xor	CH,CH
	even
for_y:
	mov	BX,y_len_256
	add	BX,y_rate
	mov	y_len_256l,bl
	test	BH,BH		; この6行ではy縮小のときに消える
	jnz	short _1	; ラインの処理をスキップして、
	mov	bl,x_bytes	; 高速化を図っている。
	add	SI,BX		;
	jmp	short next_y	;
_1:				;
	mov	y1_pos,AX
	add	AL,BH
	adc	AH,0
	dec	AX
	mov	y2_pos,AX

	mov	x_len_256,128	; 偏りを無くす
	mov	DI,org_x	; DIはx方向の位置
	mov	CH,x_bytes
	even
for_x:
	mov	AL,ES:[SI]
	mov	plane_data,AL

	push	ES
	push	CX

	mov	CX,8		; 8bit分繰返し
	even
for_bit:
	push	CX

	; カラーを計算
	shl	plane_data,1
	sbb	AX,AX

	mov	BX,x_len_256
	add	BX,x_rate
	mov	x_len_256l,bl
;	test	BH,BH		; この2行はx縮小のときに消える
;	jz	short next_bit	; ドットの処理を省くためのもの…

	mov	DX,DI
	mov	CL,BH
	add	DI,CX

	test	AX,AX
	jz	short next_bit	; ドットがないときはスキップ

	dec	DI
	push	DX
	push	y1_pos
	push	DI
	push	y2_pos
	_call	GRCG_BOXFILL
	inc	DI

	even
next_bit:
	pop	CX
	loop	short for_bit

next_x:	pop	CX
	pop	ES
	inc	SI
	dec	CH
	jnz	short for_x

	mov	AX,y2_pos
	inc	AX
	even
;	next_y:	loop	for_y
next_y:	dec	CX
	jz	short return
	jmp	for_y

return:
	_call	GRCG_OFF
	MRETURN
endfunc			; }

END
