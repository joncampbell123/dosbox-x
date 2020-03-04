; master library - VGA 16color
;
; Description:
;	パターンの拡大／縮小表示
;
; Function/Procedures:
;	void vga4_super_zoom_put(int x,int y,int num,
;				unsigned x_rate,int y_rate);
;
; Parameters:
;	x,y	左上座標
;	num	パターン番号
;	x_rate	横の倍率(xrate/256倍)
;	y_rate	横の倍率(yrate/256倍) (負ならば縦反転)
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
;	恋塚
;
; Revision History:
;	93/11/28 Initial: superzpt.asm/master.lib 0.22
;	94/ 6/ 6 Initial: vg4spzpt.asm/master.lib 0.23
;	95/ 1/ 5 [M0.23] y_rateを負にしたときに縦逆転する`
;
	.186
	.MODEL SMALL
	include func.inc

	.DATA
	extrn	super_patsize:WORD,super_patdata:WORD

	.CODE
	extrn	VGC_SETCOLOR:CALLMODEL
	extrn	VGC_BOXFILL:CALLMODEL

	.CODE

MRETURN macro
	pop	DI
	pop	SI
	leave
	ret	5*2
	EVEN
	endm

func VGA4_SUPER_ZOOM_PUT	; vga4_super_zoom_put() {
	enter	18,0

	; 引数
	org_x	= (RETSIZE+5)*2
	org_y	= (RETSIZE+4)*2
	num	= (RETSIZE+3)*2
	x_rate	= (RETSIZE+2)*2
	y_rate	= (RETSIZE+1)*2

	; ローカル変数
	pat_bytes = -2
	y1_pos 	  = -4
	y2_pos 	  = -6
	x_len_256 = -8
	y_len_256 = -10
	b_plane	  = -11
	r_plane	  = -12
	g_plane	  = -13
	i_plane	  = -14
	x_bytes	  = -15
	line_step = -18

	push	SI
	push	DI


; パターンサイズ、アドレス
	mov	BX,[BP+num]
	add	BX,BX		; integer size & near pointer
	mov	CX,super_patsize[BX]	; pattern size (1-8)
	mov	[BP+x_bytes],CH	; x方向のバイト数
				; CL には ydots
	mov	AL,CH
	mul	CL
	mov	[BP+pat_bytes],AX	; 1プレーン分のパターンバイト数
	mov	ES,super_patdata[BX]
				; パターンデータのセグメント


	mov	SI,AX		; SIはパターン内のオフセット
				; (マスクパターンをスキップ)

	mov	AX,0
	test	word ptr [BP+y_rate],8000h
	jns	short NORMAL_V
	; 縦逆転
	neg	word ptr [BP+y_rate]	; y_rate = -y_rate ;
	mov	AL,CH
	add	SI,SI		; SI = SI*2 - x_bytes ;
	sub	SI,AX		; line_step = -x_bytes * 2 ;
	add	AX,AX
NORMAL_V:
	mov	[BP+line_step],AX ; パターンアドレスの次lineに進むときの減算

	mov	word ptr [BP+y_len_256],128	; 偏りを無くす
	mov	AX,[BP+org_y]
	xor	CH,CH
	even
for_y:
	mov	BX,[BP+y_len_256]
	add	BX,[BP+y_rate]
	mov	[BP+y_len_256],BL
	test	BH,BH		; この6行ではy縮小のときに消える
	jnz	short _1	; ラインの処理をスキップして、
	mov	BL,[BP+x_bytes]	; 高速化を図っている。
	add	SI,BX		;
	sub	SI,[BP+line_step]
	loop	short for_y	;
	jmp	return		;
_1:				;
	mov	[BP+y1_pos],AX
	add	AL,BH
	adc	AH,0
	dec	AX
	mov	[BP+y2_pos],AX

	mov	word ptr [BP+x_len_256],128	; 偏りを無くす
	mov	DI,[BP+org_x]	; DIはx方向の位置
	mov	CH,[BP+x_bytes]
	even
for_x:
	mov	AL,ES:[SI]
	mov	[BP+b_plane],AL
	mov	BX,[BP+pat_bytes]
	mov	AL,ES:[SI + BX]
	mov	[BP+r_plane],AL
	add	BX,BX
	mov	AL,ES:[SI + BX]
	mov	[BP+g_plane],AL
	add	BX,[BP+pat_bytes]
	mov	AL,ES:[SI + BX]
	mov	[BP+i_plane],AL

	push	ES
	push	CX

	mov	CX,8		; 8bit分繰返し
	even
for_bit:
	push	CX

	; カラーを計算
	shl	byte ptr [BP+i_plane],1
	rcl	AL,1
	shl	byte ptr [BP+g_plane],1
	rcl	AL,1
	shl	byte ptr [BP+r_plane],1
	rcl	AL,1
	shl	byte ptr [BP+b_plane],1
	rcl	AL,1

	mov	BX,[BP+x_len_256]
	add	BX,[BP+x_rate]
	mov	[BP+x_len_256],BL
;	test	BH,BH		; この2行はx縮小のときに消える
;	jz	short next_bit	; ドットの処理を省くためのもの…

	mov	DX,DI
	mov	CL,BH
	add	DI,CX

	and	AX,0fh
	jz	short next_bit	; ドットがないときはスキップ

	push	DX		; x1
	push	[BP+y1_pos]	; y1
	dec	DI
	push	DI		; x2
	inc	DI
	push	[BP+y2_pos]	; y2

	push	00f0h		; PSET, all plane
	push	AX		; color
	_call	VGC_SETCOLOR
	_call	VGC_BOXFILL

	even
next_bit:
	pop	CX
	loop	short for_bit

next_x:	pop	CX
	pop	ES
	inc	SI
	dec	CH
	jnz	short for_x

	sub	SI,[BP+line_step]
	mov	AX,[BP+y2_pos]
	inc	AX
	even
next_y:	
	;loop	for_y
	dec	CX
	jz	short return
	jmp	for_y

return:
	MRETURN

endfunc		; }

END
