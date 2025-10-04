PAGE 98,120
; graphics - gdc - line - PC98V
;
; Description:
;	GDCによる直線の描画
;
; Function/Proeceudre:
;	void pascal gdc_line( int x1, int y1, int x2, int y2 ) ;

; Parameters:
;	int x1,y1	始点の座標
;	int x1,y2	終点の座標
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal..
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCELARATOR: GDC
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	・CPUの動作と並列して描画します。このルーチンから戻っても
;	　まだ描画が完了していません。描画完了を待つには、
;	　gdc_wait()を実行して下さい。
;
; Author:
;	恋塚昭彦
;
; 関連関数:
;	gdc_wait()
;
; History:
;	92/ 7/27 Initial
;	92/10/ 9 line style逆転補正
;	92/10/26 I/Oガードタイム修正(一部機種で異常動作したので)
;	93/ 5/13 [M0.16] gdc_outpw, GDC_WAITEMPTYを外部に出した

	.186
	.MODEL SMALL

GDC_PSET equ 0
GDC_XOR	equ 1
GDC_AND	equ 2
GDC_OR	equ 3

	.DATA

	; クリッピング関係
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD

	EXTRN	GDCUsed:WORD
	EXTRN	GDC_LineStyle:WORD
	EXTRN	GDC_AccessMask:WORD
	EXTRN	GDC_Color:WORD

	.DATA?

DGD	DW ?
DotAdr	DB ?
Dir	DB ?

	.CODE
	include func.inc
	include clip.inc

; GDC modify mode
REPLACE	   equ 020h
COMPLEMENT equ 021h
CLEAR	   equ 022h
SET	   equ 023h

VECTW	equ 4ch
VECTE	equ 6ch
TEXTW	equ 78h
CSRW 	equ 49h

	EXTRN GDC_WAITEMPTY:CALLMODEL
	EXTRN gdc_outpw:NEAR

; IN:
;	SI : adx
;	BP : ady2
linedelta	PROC NEAR
	call	GDC_WAITEMPTY		; FIFOが空になるのを待つ
	mov	AL,VECTW
	out	0a2h, AL		; [C 1]

	mov	AL,Dir
	out	0a0h, AL		; [P 1]

	mov	AX,SI
	or	AX,DGD
	call	gdc_outpw		; [P 2]

	mov	AX,BP
	sub	AX,SI
	call	gdc_outpw		; [P 2]

	mov	AX,SI
	shl	AX,1
	sub	AX,BP
	neg	AX
	call	gdc_outpw		; [P 2]

	mov	AX,BP
	call	gdc_outpw		; [P 2]

	ret
	EVEN
linedelta	ENDP


; IN:
;	AX: top
; BREAK:
;	AX
drawline	PROC NEAR
	push	AX
	call	GDC_WAITEMPTY		; FIFOが空になるのを待つ
	mov	AL,CSRW
	out	0a2h, AL		; [C 1]

	pop	AX
	call	gdc_outpw		; [P 2]
	mov	AL,DotAdr
	out	0a0h, AL		; [P 1]

	mov	AL,VECTE
	out	0a2h, AL		; [C 1]

	ret
	EVEN
drawline	ENDP

;
;
MRETURN macro
	pop	SI
	pop	DI
	pop	BP
	ret	8
	EVEN
	endm

func GDC_LINE
	push	BP
	mov	BP,SP
	push	DI
	push	SI
;	register DX = work

	; 引数
	CLI
	; parameters
	add	SP,(RETSIZE+3)*2
	pop	BP	; y2
	pop	SI	; x2
	pop	DI	; y1
	pop	CX	; x1
	sub	SP,(RETSIZE+3+4)*2
	STI

	; クリッピング開始 =================================

	; クリップ枠に合わせてy座標を変換
	mov	AX,ClipYT
	sub	DI,AX
	sub	BP,AX

	mov	AX,ClipXL
	mov	DX,ClipYH
	mov	BX,0505h	; outcode を全部左下に!!
	GETOUTCODE BL,CX,DI,AX,ClipXR,DX
	GETOUTCODE BH,SI,BP,AX,ClipXR,DX
	test	BH,BL
	jnz	short RETURN1	; まず初期検査。クリップアウト～
	or	BX,BX
	jz	short DRAW_START

	call	cutline		; 第１点のクリップ
	jz	short DRAW_START
	xchg	BH,BL
	xchg	CX,SI
	xchg	DI,BP
	call	cutline		; 第２点のクリップ
	jnz	short RETURN1

	xchg	CX,SI		; 前後関係をもとにもどす
	xchg	DI,BP		; (ラインスタイルの逆転を防ぐため)

	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+

	; 描画開始 =================================
DRAW_START:
	mov	AX,ClipYT
	add	DI,AX
	add	BP,AX

	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[054dh],4	; NZ = GDC 5MHz
	jz	short L100		; Z = 2.5MHz
	mov	DGD,4000h
	jmp	short L200
RETURN1:
	MRETURN
L100:
	mov	DGD,0
	cmp	GDCUsed,0
	je	short L200
	call	GDC_WAITEMPTY
L200:
	mov	AL,TEXTW
	out	0a2h, AL		; ラインスタイルの設定 [ C 1 ]

	mov	AX,GDC_LineStyle
	call	gdc_outpw		; ラインスタイルの設定 [ P 2 ]

	;	44332
	;	5-  2
	;	5 o 1 : Dir( direction )
	;	6  +1
	;	67700
	;
	sub	BP,DI
	sub	SI,CX
	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  !x2-x1! y1  !y2-y1!
	; +-----+-----+-----+-----+-----+-----+-----+
	; or	SI,SI
	jle	short L300
	or	BP,BP
	jl	short L300

	mov	Dir,8
	jmp	short L600
	EVEN
L300:
	neg	SI
	or	SI,SI
	jg	short L400
	or	BP,BP
	jge	short L400

	mov	Dir,10
	jmp	short L600
	EVEN
L400:
	neg	BP
	or	BP,BP
	jl	short L500
	or	SI,SI
	jle	short L500
	mov	Dir,12
	jmp	short L600
	EVEN
L500:
	neg	SI
	mov	Dir,14
L600:
	cmp	BP,SI
	jge	short L700
	inc	Dir
L700:
	mov	AX,SI
	cwd
	xor	AX,DX
	sub	AX,DX
	mov	SI,AX
	mov	AX,BP
	cwd
	xor	AX,DX
	sub	AX,DX
	mov	BP,AX
	cmp	BP,SI
	jle	short L800
	xchg	SI,BP
L800:
	shl	BP,1
	call	linedelta

	mov	DX,DI		; DX = (y1*40)+(x1/16)
	mov	AX,CX		;      (GDC ADDRESS)
	shl	DX,2		;
	add	DX,DI		;
	shl	DX,3		;
	shr	CX,4		;
	add	DX,CX		;

	shl	AL,4
	mov	DotAdr,AL

	mov	BX,GDC_AccessMask
	not	BL
	and	BL,0fh
	mov	CX,GDC_Color
	dec	CH
	jns	short XORDRAW

	; PSET描画 -----------------------
NORMALDRAW:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short RED

	out	0a2h, AL
	mov	AX,DX
	or	AH,40h	; GDC VRAM ADDRESS: Blue
	call	drawline
	call	linedelta
RED:

	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short GREEN

	out	0a2h, AL
	mov	AX,DX
	or	AH,80h	; GDC VRAM ADDRESS: Red
	call	drawline
	call	linedelta

GREEN:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short INTENSITY

	out	0a2h, AL
	mov	AX,DX
	or	AH,0C0h	; GDC VRAM ADDRESS: Green
	call	drawline
	call	linedelta

INTENSITY:
	xor	AL,AL
	shr	CL,1
	adc	AL,CLEAR
	shr	BL,1
	jnb	short OWARI

	out	0a2h, AL
	jmp	short LLAST
	EVEN

	; XOR,AND,OR描画 -------------------
XORDRAW:
	mov	AL,CH
	add	AL,COMPLEMENT
	out	0a2h, AL

	cmp	CH,GDC_AND-1
	jne	short NOTAND
	not	CL
NOTAND:
	and	CL,BL
	jz	short OWARI

	shr	CL,1
	jnb	short XRED
	mov	AX,DX
	or	AH,40h	; GDC VRAM ADDRESS: Blue
	call	drawline
	call	linedelta
XRED:
	shr	CL,1
	jnb	short XGREEN
	mov	AX,DX
	or	AH,80h	; GDC VRAM ADDRESS: Red
	call	drawline
	call	linedelta
XGREEN:
	shr	CL,1
	jnb	short XINTENSITY
	mov	AX,DX
	or	AH,0C0h	; GDC VRAM ADDRESS: Green
	call	drawline
	call	linedelta
XINTENSITY:
	shr	CL,1
	jnb	short OWARI
LLAST:
	mov	AX,DX	; GDC VRAM ADDRESS: Intensity
	call	drawline
OWARI:
	mov	GDCUsed,1
	MRETURN
endfunc

END
