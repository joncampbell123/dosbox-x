; superimpose & master library module
;
; Description:
;	パターンの表示 [横8dot単位][yクリッピング]
;
; Functions/Procedures:
;	void super_put_clip_8( int x, int y, int num ) ;
;
; Parameters:
;	x	x座標 0～639(8dot単位左寄せ)
;	y	y座標 0-(パターン高さ-1)～399
;	num	パターン番号
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
;	CPU: V30
;
; Notes:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: supermch.asm 0.03 92/05/29 20:31:03 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16] bugfix, super_put_mitchからsuper_put_clip_8に改名
;	93/ 6/26 [M0.19] (^^;
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

MRETURN	macro
	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN
	endm

func SUPER_PUT_CLIP_8
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	AX,[BP+y]
	mov	BX,[BP+num]

	mov	DI,AX		; DI = y
	mov	BP,AX		;-+
	shl	AX,2		; |
	add	BP,AX		; |BP=y*80
	shl	BP,4		;-+
	shr	CX,3		;DX=x/8
	add	BP,CX		;GVRAM offset address

	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	DS,super_patdata[BX]
	mov	AL,80
	sub	AL,DH

	mov	BL,DH		; BL = pattern width(byte)
	mov	BH,AL		; BH = next line..
	mov	DH,0		; DX = pattern height(dot)

	test	DI,DI
	jns	short PLUS
	add	DX,DI
	jle	short RETMAIN
	neg	DI
	mov	AH,0
	mov	AL,BL
	mov	SI,DX
	mul	DI
	mov	DX,SI
	mov	SI,AX
	mov	BP,CX
	jmp	short OVER_END
RETMAIN:
	MRETURN
PLUS:
	xor	AX,AX
	mov	SI,AX
	cmp	DI,400
	jge	short RETMAIN
	add	DI,DX
	cmp	DI,400
	jl	short OVER_END
	lea	AX,[DI-400]	; AX = (y+height) - 400	表示しない部分の長さ
	sub	DX,AX		; DX = 400 - y 表示部分の長さ
	mov	DI,DX
	mov	CH,0
	mov	CL,BL
	mul	CX
	mov	DX,DI

OVER_END:
	mov	WORD PTR CS:[_OVER_],AX

	mov	AX,0a800h
	mov	ES,AX

	mov	AH,DL		; AH = pattern height(dot) [clipped]

	mov	DL,BH		; DX = next line..
	mov	DH,0

	mov	BH,0		; BX = pattern width(byte)

	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,0
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP		;cls

	mov	AL,11001110b
	out	7ch,AL		;RMW mode
	mov	AL,0ffh		;AL==0ffh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP
	mov	AL,11001101b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	AL,11001011b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	AL,11000111b
	out	7ch,AL		;RMW mode
	call	DISP

	xor	AL,AL
	out	7ch,AL		;grcg off
	MRETURN
endfunc

;
;
; in:
;	ES:BP = vram address
;	AH = pattern height
;	BX = pattern width
;	DX = 80 - pattern width
;	DS:SI = pattern address
DISP	proc	near
	mov	DI,BP
	mov	AL,AH
LOOPS:
	mov	CX,BX
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	add	DI,DX
	dec	AL
	jnz	short LOOPS
	add	SI,1111h	;dummy
_OVER_		EQU	$-2
	ret
DISP	endp

END
