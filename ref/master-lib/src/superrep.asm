; superimpose & master library module
;
; Description:
;	パターンの消去 [裏画面] [1dot単位] [マスクパターン]
;
; Functions/Procedures:
;	void super_repair( int x, int y, int num ) ;
;
; Parameters:
;	x,y	左上の座標
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
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
;
;$Id: superrep.asm 0.11 92/05/29 20:39:05 Kazumi Rel $
;
	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	super_buffer:WORD

	.CODE

	; マスクパターンを一旦溜めるところ
buffer		DB	2304 dup (?)
	EVEN

func SUPER_REPAIR
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	DX,[BP+x]
	mov	AX,[BP+y]
	mov	BX,[BP+num]

	mov	DI,AX		;-+
	shl	DI,2		; |SI=y*80
	add	DI,AX		; |
	shl	DI,4		;-+
	mov	AX,DX
	and	AX,7
	mov	BYTE PTR CS:[BIT_SHIFT],AL
	shr	DX,3		;AX=x/8
	add	DI,DX		;GVRAM offset address
	mov	BP,DI
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)

	push	BX
	push	DX
	push	DS
	mov	AL,1
	out	0a6h,AL

	mov	ES,super_buffer
	xor	DI,DI
	mov	CL,DH
	add	CL,2
	and	CL,0feh
	mov	CH,80
	sub	CH,CL
	mov	BYTE PTR CS:[NEXT_BLUE],CH
	mov	BYTE PTR CS:[NEXT_RED],CH
	mov	BYTE PTR CS:[NEXT_GREEN],CH
	mov	BYTE PTR CS:[NEXT_INTEN],CH
	shr	CL,1
	xor	CH,CH
	mov	BX,CX
	mov	DH,DL

	mov	AX,0a800h
	mov	DS,AX
	mov	SI,BP
BLUE:
	rep	movsw
	mov	CX,BX
	add	SI,11h
NEXT_BLUE	EQU	$-1
	dec	DH
	jnz	short BLUE
	mov	DH,DL
	mov	AX,0b000h
	mov	DS,AX
	mov	SI,BP
RED:
	rep	movsw
	mov	CX,BX
	add	SI,11h
NEXT_RED	EQU	$-1
	dec	DH
	jnz	short RED
	mov	DH,DL
	mov	AX,0b800h
	mov	DS,AX
	mov	SI,BP
GREEN:
	rep	movsw
	mov	CX,BX
	add	SI,11h
NEXT_GREEN	EQU	$-1
	dec	DH
	jnz	short GREEN
	mov	DH,DL
	mov	AX,0e000h
	mov	DS,AX
	mov	SI,BP
INTEN:
	rep	movsw
	mov	CX,BX
	add	SI,11h
NEXT_INTEN	EQU	$-1
	dec	DH
	jnz	short INTEN
	mov	AL,DH		;AL = 0
	out	0a6h,AL
	pop	DS
	pop	DX
	pop	BX

	push	DS
	mov	DS,super_patdata[BX]
	mov	AX,DX
	xor	AH,AH
	mul	DH
	mov	CX,AX
	mov	DI,OFFSET buffer
	mov	AX,CS
	mov	ES,AX
	xor	SI,SI
	mov	AX,CX
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb
	pop	DS

	mov	DI,BP
	xor	SI,SI
	mov	DS,super_buffer
	mov	CH,DH
	mov	BYTE PTR CS:[XCOUNT],CH
	mov	AL,80
	sub	AL,CH
	mov	BYTE PTR CS:[NEXT_LINE],AL
	mov	AH,1
	mov	AL,DH
	and	AL,1
	jnz	short NO_INC
	inc	AH
NO_INC:
	mov	BYTE PTR CS:[SKIP_BYTE],AH
	mov	CL,11h
BIT_SHIFT	EQU	$-1
	mov	DI,BP
	mov	DH,DL
	mov	AX,0a800h
	call	REPAIR
	mov	DI,BP
	mov	DH,DL
	mov	AX,0b000h
	call	REPAIR
	mov	DI,BP
	mov	DH,DL
	mov	AX,0b800h
	call	REPAIR
	mov	DI,BP
	mov	DH,DL
	mov	AX,0e000h
	call	REPAIR

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc

;
;
;
REPAIR	proc	near
	mov	ES,AX
	mov	BX,OFFSET buffer
	EVEN
REPAIR_LOOP:
	mov	AL,CS:[BX]
	xor	AH,AH
	ror	AX,CL
	not	AX
	and	ES:[DI],AX
	not	AX
	and	AX,[SI]
	or	ES:[DI],AX
	inc	BX
	inc	SI
	inc	DI
	dec	CH
	jnz	short REPAIR_LOOP

	add	DI,80		;dummy
NEXT_LINE	EQU	$-1
	add	SI,0		;dummy
SKIP_BYTE	EQU	$-1
	mov	CH,11h
XCOUNT		EQU	$-1
	dec	DH
	jnz	short REPAIR_LOOP
	ret
REPAIR	endp

END
