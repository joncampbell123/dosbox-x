; Description:
;	マウスドライバ(第２層)
;		ハードウェアアクセスとイベント管理
;
; Functions/Procedures:
;	void mouse_proc_init( void ) ;
;	void mouse_resetrect( void ) ;
;	void mouse_setrect( int x1, int y1, int x2, int y2 ) ;
;	int far mouse_proc( void ) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial
;	93/12/10 [M0.22] BUGFIX mouse_EventRoutine時, 正しくDXにyが入ってない

	.MODEL SMALL
	include func.inc

	.DATA?

	EXTRN graph_VramLines:WORD

	EXTRN mouse_X:WORD, mouse_Y:WORD
	EXTRN mouse_Button:WORD
	EXTRN mouse_ScaleX:WORD
	EXTRN mouse_ScaleY:WORD
	EXTRN mouse_EventRoutine:DWORD
	EXTRN mouse_EventMask:WORD

cursor_x_dif	dw	?
cursor_y_dif	dw	?

	.CODE

; Ｘ座標の更新
; in: AX
	public mouse_set_x
mouse_set_x	PROC NEAR
	mov	DX,0
org	$-2
area_x1	dw	0
	cmp	AX,DX
	jl	short HOSEIX
	mov	DX,639
org	$-2
area_x2	dw	639
	cmp	AX,DX
	jle	short NEXTX
HOSEIX:	mov	AX,DX
	mov	cursor_x_dif,0
NEXTX:
	mov	mouse_X,AX
	ret
mouse_set_x	ENDP

; Ｙ座標の更新
;in: AX
	public mouse_set_y
mouse_set_y	PROC NEAR
	mov	DX,0
org	$-2
area_y1	dw	0

	cmp	AX,DX
	jl	short HOSEIY

	mov	DX,399
org	$-2
area_y2	dw	399

	cmp	AX,DX
	jle	short NEXTY
HOSEIY:	mov	AX,DX
	mov	cursor_y_dif,0
NEXTY:
	mov	mouse_Y,AX
	ret
mouse_set_y	ENDP

func MOUSE_PROC_INIT
	mov	mouse_X,320
	mov	AX,graph_VramLines
	shr	AX,1
	mov	mouse_Y,AX
	mov	mouse_ScaleX,8

	mov	BX,8
	cmp	AX,200 shr 1
	jg	short INIT_S
	shl	BX,1
INIT_S:
	mov	mouse_ScaleY,BX

	xor	AX,AX
	mov	mouse_Button,AX
	mov	cursor_x_dif,AX
	mov	cursor_y_dif,AX
	mov	mouse_EventMask,AX
	call	CALLMODEL PTR MOUSE_RESETRECT

	mov	DX,07fdfh	; マウスのカウンタのクリア
	mov	AL,0eh
	out	DX,AL

	mov	AL,0fh
	out	DX,AL

	mov	AL,0eh
	out	DX,AL
	ret
endfunc

func MOUSE_RESETRECT
	xor	AX,AX
	mov	CS:area_x1,AX
	mov	CS:area_y1,AX
	mov	CS:area_x2,639
	mov	AX,graph_VramLines
	dec	AX
	mov	CS:area_y2,AX
	out	64h,AL		; reset vsync for cpu cache flash
	ret
endfunc

func MOUSE_SETRECT
	push	BP
	mov	BP,SP
	; 引数
	x1	= (RETSIZE+4)*2
	y1	= (RETSIZE+3)*2
	x2	= (RETSIZE+2)*2
	y2	= (RETSIZE+1)*2

	mov	AX,[BP+x1]
	mov	CS:area_x1,AX
	mov	AX,[BP+y1]
	mov	CS:area_y1,AX
	mov	AX,[BP+x2]
	mov	CS:area_x2,AX
	mov	AX,[BP+y2]
	mov	CS:area_y2,AX
	out	64h,AL		; reset vsync for cpu cache flash

	pop	BP
	ret 8
endfunc

	public MOUSE_PROC
MOUSE_PROC proc far
	push	SI
	push	DI
	pushf
	CLI
	mov	DI,7fdfh - 2
	mov	SI,7fdfh - 2 - 4

	mov	AL,90h	; xd low4bit, HC=1,~INT=1
	mov	DX,DI
	out	DX,AL
	mov	DX,SI
	in	AL,DX
	and	AL,0fh
	mov	BL,AL

	mov	AL,0b0h	; xd upper4bit, HC=1,~INT=1
	mov	DX,DI
	out	DX,AL
	mov	DX,SI
	in	AL,DX
	mov	CL,4
	shl	AL,CL
	or	AL,BL
	cbw
	mov	BX,AX	; BX = Ｘ移動量

	mov	AL,0d0h	; yd low4bit読み取り指定, HC=1,~INT=1
	mov	DX,DI
	out	DX,AL
	mov	DX,SI
	in	AL,DX	; yd low4bit読み取り
	and	AL,0fh
	mov	CL,AL

	mov	AL,0f0h	; yd upper4bit読み取り指定, HC=1,~INT=1
	mov	DX,DI
	out	DX,AL
	mov	DX,SI
	in	AL,DX	; yd upper4bit読み取り
	mov	CH,AL	; buttons
	shl	AL,1
	shl	AL,1
	shl	AL,1
	shl	AL,1
	or	AL,CL
	cbw
	xchg	CX,AX	; CX = Ｙ移動量, AH=buttons

	mov	AL,10h	; HC=0,~INT=1
	mov	DX,DI
	out	DX,AL


	; ボタンの取得
	mov	AL,AH
	and	AL,20h	; 20h RIGHT
	cmp	AL,20h
	mov	AL,0
	rcl	AL,1
	shl	AH,1	; 80h LEFT
	cmc
	rcl	AL,1

NO_EVENT	equ	00001000b

	mov	AH,byte ptr mouse_Button
	mov	byte ptr mouse_Button,AL
	mov	DL,AH
	xor	DL,AL		; 変化 ---
	mov	DH,DL
	and	DX,AX		; [7] [6] [5] [4]  [3] [2] [1] [0]
	ror	DL,1		;  |   |   |  LDN   |   |   |   |  Lbutton Down
	shr	DH,1		;  |   |   |        |   |   |   |
	rcr	DL,1		;  |   |   LUP      |   |   |   |  LButton Up
	rcr	DL,1		;  |  RDN           |   |   |   |  Rbutton Down
	shr	DH,1		;  |                |   |   |   |
	rcr	DL,1		; RUP               |   |   |   |  RButton Up
	or	AL,DL		;                   |   |  BR  BL  Button
				;                   |   |
	mov	DX,BX		;                   |   |
	or	DX,CX		;                   |   |
	add	DX,-1		;                   |   |
	sbb	DL,DL		;                   |   |
	and	DL,4		;                   |  MOV	   Moved
	or	AL,DL		;                   |
				;                   |
	cmp	AL,4		; 011111100bが イベントマスクなので
	sbb	DL,DL		;                   |
	and	DL,NO_EVENT	;		   NEV		   No Event
	or	AL,DL

	; AL = event
	mov	AH,0
	mov	SI,AX
	; SI = event

	mov	DX,mouse_X
	shl	BX,1
	jz	short DXZERO
		shl	BX,1
		shl	BX,1
		mov	AX,cursor_x_dif
		xchg	AX,BX
		add	AX,BX
		mov	BX,DX
		cwd
		idiv	mouse_ScaleX
		mov	cursor_x_dif,DX
		add	AX,BX
		call	mouse_set_x
		mov	DX,AX
DXZERO:
	mov	BX,mouse_Y
	shl	CX,1
	jz	short DYZERO
		shl	CX,1
		shl	CX,1
		mov	AX,cursor_y_dif
		xchg	AX,CX
		add	AX,CX
		push	DX
		cwd
		idiv	mouse_ScaleY
		mov	cursor_y_dif,DX
		add	AX,BX
		call	mouse_set_y
		pop	DX
		mov	BX,AX
	DYZERO:

	mov	AX,SI		; AX = EVENT MASK
				; DX = x(scaled)
				; BX = y(scaled)

	popf
	test	AX,mouse_EventMask
	jz	short IGNORE_EVENT
	call	DWORD PTR mouse_EventRoutine	; イベントルーチンを呼ぶ
IGNORE_EVENT:
	mov	AX,SI

	pop	DI
	pop	SI
	ret
MOUSE_PROC endp

END
