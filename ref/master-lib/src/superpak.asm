; master library - superimpose packedpixel - entry
;
; Description:
;	パック16色データからパターンを登録する
;
; Functions/Procedures:
;	int super_entry_pack( const void far * image, unsigned image_width, int patsize, int clear_color ) ;
;
; Parameters:
;	image		packed dataの読み取り開始アドレス
;	image_width	packed data全体の横ドット数(2dot単位, 端数は切り上げ)
;	patsize		登録するパターンの大きさ
;	clear_color	透明色
;
; Returns:
;	0～511		     登録されたパターン番号
;	InsufficientMemory   メモリが足りない
;	GeneralFailure	     パターンの登録数が多すぎる
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
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
;	93/11/22 Initial: superpak.asm/master.lib 0.21
;	95/ 4/ 3 [M0.22k] 引き数の検査をすこし厳しくした

	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	SUPER_ENTRY_PAT:CALLMODEL
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
	endm

retfunc FAILURE
	mov	AX,GeneralFailure
NOMEM:
	STC
	MRETURN
endfunc

func SUPER_ENTRY_PACK	; super_entry_pack() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	image	    = (RETSIZE+4)*2
	image_width = (RETSIZE+3)*2
	patsize	    = (RETSIZE+2)*2
	clear_color = (RETSIZE+1)*2

	mov	AX,[BP+patsize]		; SI = patsize( hi:Xdots, lo:Ydots )
	mov	SI,AX
	mov	CL,AH
	mov	CS:xbytes,CL
	mov	CH,0			; CX = Xdots
	mov	AH,0
	mul	CX			; AX = size of a plane
	test	DX,DX
	jnz	short FAILURE
	mov	DX,AX			; DX = plane size
	shl	AX,1			; 4plane分
	jc	short FAILURE
	shl	AX,1
	jc	short FAILURE
	mov	BX,AX			; BX = 4plane分のバイト数
	push	AX
	call	SMEM_WGET
	jc	short NOMEM

	mov	ES,AX			; ES = segment of temporary buffer
	xor	BX,BX

	shl	CX,1
	shl	CX,1			; CX = Xdots*4(バイト数)
	mov	DI,[BP+image_width]
	shr	DI,1
	adc	DI,BX			; BX=0
	sub	DI,CX			; DI = image_width/2 - 横バイト数
					; これにより、imageの1lineの差を出す

	mov	CX,SI			; CX = patsize

	; super_entry_patの引数を先にpush
	push	CX			; patsize _patsize
	push	ES			; seg addr
	push	BX			; offset address = 0
	push	[BP+clear_color]	; clear_colorは引数そのまま

	push	DS			; DS を待避
	lds	SI,[BP+image]

	mov	BP,DI			; BPに、imageの1lineの差を写す

next_y:
	mov	CH,0	; dummy
	org $-1
xbytes	db	?

next_x:
	push	CX
	push	DX
	push	BX
	lodsw
	mov	BX,AX
	lodsw
	mov	DX,AX			; BL BH DL DH
	mov	DI,4
B2V_LOOP:
	rol	BL,1
	RCL	CH,1
	rol	BL,1
	RCL	CL,1
	rol	BL,1
	RCL	AH,1
	rol	BL,1
	RCL	AL,1

	rol	BL,1
	RCL	CH,1
	rol	BL,1
	RCL	CL,1
	rol	BL,1
	RCL	AH,1
	rol	BL,1
	RCL	AL,1

	mov	BL,BH
	mov	BH,DL
	mov	DL,DH

	dec	DI
	jnz	short B2V_LOOP

	pop	BX
	pop	DX

	; DI=0だし
	mov	ES:[BX+DI],AL
	add	DI,DX
	mov	ES:[BX+DI],AH
	add	DI,DX
	mov	ES:[BX+DI],CL
	add	DI,DX
	mov	ES:[BX+DI],CH
	pop	CX

	inc	BX
	dec	CH
	jnz	short next_x

	add	SI,BP			; imageを次のlineに進める
	; CH=0だし
	loop	short next_y

	pop	DS			; DSを復元

	mov	SI,ES			; SIに開放するsegmentを待避

	; 引数はすでにpushしてあるからそのままcall
	call	SUPER_ENTRY_PAT		; (patsize, far addr, clear_color)
	push	SI
	call	SMEM_RELEASE		; この関数はAXもflagも保存されている
	MRETURN
endfunc		; }

END
