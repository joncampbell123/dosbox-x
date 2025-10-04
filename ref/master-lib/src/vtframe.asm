; master library - vtext
;
; Description:
;	テキスト画面に窓枠を描画する
;
; Functions/Procedures:
;	void vtext_frame(int x1, int y1, int x2, int y2,
;			 unsigned wattr, unsigned iattr, int round ); 
;
; Parameters:
;	x1,y1	左上の座標
;	x2,y2	右下の座標
;	wattr	窓枠の属性
;	iattr	中の属性
;	round	dummy
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	窓枠を描画します。
;	窓枠自体は wattrの属性で、まどの中はiattrの属性で塗ります。
;	窓の中身は空白で塗り潰します。
;	窓の両脇に漢字の半分がかかっていた場合、その漢字も空白で
;	塗り潰します。
;
;	クリッピングや値範囲の検査は一切おこなっていません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
;
; Revision History:
;	93/ 8/22 Initial
;	93/10/ 4 桁を work からではなく text_begin で得た値で処理するようにした
;	93/11/ 7 VTextState 導入
;	94/01/16 text_window
;	94/  /   _call 導入
;	94/02/13 _call --> call へ
;	94/ 4/ 9 Initial: vtframe.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramAdr : DWORD
	EXTRN TextVramSize : WORD
	EXTRN TextVramWidth : WORD
	EXTRN Machine_State : WORD
	EXTRN VTextState : WORD

	.CODE

func VTEXT_FRAME	; vtext_frame() {
	push	BP
	mov	BP,SP

	; 引数
	x1	= (RETSIZE+7)*2
	y1	= (RETSIZE+6)*2
	x2	= (RETSIZE+5)*2
	y2	= (RETSIZE+4)*2
	batr	= (RETSIZE+3)*2
	iatr	= (RETSIZE+2)*2
	round	= (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	CLD
	mov	CX,TextVramWidth

	mov	BH,[BP + x2]
	inc	BH		; BH= x2
	mov	BL,[BP + y2]
;	dec	BL		; BL= y2

	mov	AL,[BP + y1]
	sub	BL,AL

	mul	CL		; AX= top address
	mov	DI,[BP + x1]	; DI= 左端
	mov	CX,DI		; CX= DI
	sub	BH,CL		; x 方向の文字数(長さ)
	add	DI,AX		; 左上の box のaddress
	shl	DI,1		; 2 倍(code+atrだから)

	mov	DX,[BP + batr]	; CL= box の属性

	pop	DS

	mov	ES,word ptr TextVramAdr+2
	add	DI,word ptr TextVramAdr

	push	DI
	call	CHK_KNJ		; c...............c 漢字チェック
	test	Machine_State,0001h	; 日本語?
	mov	SI,0c9bbh
	jne	short SKIP_JP1
	mov	SI,0102h
SKIP_JP1:
	call	UP_DOWN		;  +-------------+ (上)
	pop	DI

				; |             | (中)

	mov	DH,0bah
	test	Machine_State,0001h	; 日本語?
	jne	short SKIP_JP2
	mov	DH,05h
SKIP_JP2:

	xor	AX,AX
	mov	AL,BL
	mov	SI,AX

LOOP_NAKA:
	mov	AX,TextVramWidth
	shl	AX,1
	add	DI,AX

	dec	SI
	jz	short LOOP_END
	push	SI		; [1
	push	DI		; [2
	call	CHK_KNJ

	mov	AL,DH
	mov	AH,DL
	stosw

	mov	AH,[BP + iatr]
	mov	AL,20h
	xor	CH,CH	; mov CH,0
	mov	CL,BH
	dec	CX
	dec	CX
	rep	stosw

	mov	AL,DH
	mov	AH,DL
	stosw
	call	UPDATELINE
	pop	DI		; 2]
	pop	SI		; 1]
	jmp	short LOOP_NAKA
LOOP_END:

	call	CHK_KNJ		; c...............c 漢字チェック
	test	Machine_State,0001h	; 日本語?
	mov	SI,0c8bch
	jne	short SKIP_JP3
	mov	SI,0304h
SKIP_JP3:
	call	UP_DOWN		;  +-------------+ (上)

	pop	DI
	pop	SI
	pop	BP

TEXT_WINDOW_EXIT:
	ret	7*2
endfunc			; }

; DI : 左端
; BH : 桁数
; DX : 属性
; breaks:
;	AX = atr
;	SI = DI
;	CX = 0
CHK_KNJ proc near	; 漢字の check
 ret
	test	byte ptr ES:[DI-2],80h
	jz	short SKIP_RIGHT
	mov	ES:[DI-2],0720h			; 空白
SKIP_RIGHT:

	mov	SI,DI
	xor	AH,AH	; mov AH,0
	mov	AL,BH
	shl	AX,1
	add	DI,AX
	test	byte ptr ES:[DI-2],80h
	jz	short SKIP_LEFT
	mov	ES:[DI],0720h			; 空白
SKIP_LEFT:

	mov	DI,SI
	ret
CHK_KNJ endp


;
; SI = h : 左上下 , l : 右上下
; DL = 属性
;
UP_DOWN proc near	; 上と下
	mov	AX,SI
	mov	AL,AH
	mov	AH,DL
	stosw
	test	Machine_State,0001h	; 日本語?
	mov	AL,0cdh
	jne	short SKIP_JP4
	mov	AL,06h
SKIP_JP4:
	xor	CH,CH	; mov CH,0
	mov	CL,BH
	dec	CX
	dec	CX
	rep	stosw

	mov	AX,SI
	mov	AH,DL
	stosw

	call	UPDATELINE
	ret
UP_DOWN endp

; in: ES:DI = 目的の行の書きおわった場所
;     BH = 文字数
UPDATELINE proc near
	test	VTextState,1
	jnz	short NO_REF
	mov	AL,BH
	mov	AH,0
	mov	CX,AX
	add	AX,AX
	sub	DI,AX
	mov	AH,0ffh
	int	10h		; refresh line
NO_REF:
	ret
UPDATELINE endp

END
