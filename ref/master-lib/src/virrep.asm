; superimpose & master library module
;
; Description:
;	仮想VRAMから実画面へ転送
;
; Functions/Procedures:
;	void virtual_repair( int x, int y, int num ) ;
;
; Parameters:
;	x,y 転送範囲を示すパターンの座標
;	num 転送範囲を示すパターンの番号
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
;$Id: virrep.asm 0.06 92/05/29 20:44:06 Kazumi Rel $
;                     91/09/17 01:10:10 Mikabon(ﾐ) Bug Fix??
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 5/ 4 [M0.16]

	.186
	.MODEL SMALL
	include func.inc
	.DATA

	EXTRN	super_patsize:WORD
	EXTRN	virtual_seg:WORD

	.CODE

PLANESEG equ 80*400/16

func VIRTUAL_REPAIR	; {
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
	mov	BP,DX
	shr	DX,3		;AX=x/8
	add	DI,DX		;GVRAM offset address
	mov	CS:_SI_,DI
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	mov	CL,DH
	xor	CH,CH
	and	BP,7
	neg	BP
	adc	CL,CH
	mov	AX,80
	sub	AL,CL
	mov	BX,AX
	mov	BP,CX
	mov	CX,0a800h
	mov	AX,virtual_seg
	call	REPAIR
	mov	CX,0b000h
	add	AX,PLANESEG
	call	REPAIR
	mov	CX,0b800h
	add	AX,PLANESEG
	call	REPAIR
	mov	CX,0e000h
	add	AX,PLANESEG
	call	REPAIR

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN

endfunc			; }

REPAIR	proc	near
	mov	ES,CX
	mov	DS,AX

	JMOV	SI,_SI_

	mov	DH,DL
	shr	BP,1
	jnb	short REPAIR_EVEN

	EVEN
REPAIR_ODD:
	mov	DI,SI
	mov	CX,BP
	movsb
	rep	movsw
	lea	SI,[SI+BX]
	dec	DH
	jnz	short REPAIR_ODD
	rcl	BP,1
	ret

	EVEN
REPAIR_EVEN:
	mov	DI,SI
	mov	CX,BP
	rep	movsw
	lea	SI,[SI+BX]
	dec	DH
	jnz	short REPAIR_EVEN
	rcl	BP,1
	ret
REPAIR	endp

END
