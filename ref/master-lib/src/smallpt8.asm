; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	over_small_put_8
;
; Parameters:
;	
;
; Returns:
;	
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
;	Kazumi(âúìc  êm)
;	óˆíÀ(óˆíÀè∫ïF)
;
; Revision History:
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
;
;$Id: smallpt8.asm 0.02 92/05/29 20:16:30 Kazumi Rel $
;
	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

func OVER_SMALL_PUT_8
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	shr	CX,3		;CX=x/8
	add	DI,CX		;GVRAM offset address
	mov	AX,[BP+num]
	shl	AX,1		;integer size & near pointer
	mov	BP,DI
	mov	BX,OFFSET super_patsize
	add	BX,AX
	mov	DX,[BX]		;pattern size (1-8)
	mov	BX,OFFSET super_patdata
	add	BX,AX
	mov	CX,[BX]		;BX+2 -> BX
	push	DX
	mov	AL,DH
	xor	AH,AH
	mov	BX,DX
	mov	BH,AH		;BH = 0
	mul	BX
	mov	SI,AX
	pop	DX
	mov	DS,CX
	mov	AL,80
	shr	DH,1		;/2
	sub	AL,DH
	mov	BYTE PTR CS:[next_line],AL
	mov	CL,DH
	mov	CH,CL
	shr	DL,1
	mov	DH,DL
	mov	AX,0a800h
	call	DISP
	mov	AX,0b000h
	mov	DH,DL
	mov	DI,BP
	call	DISP
	mov	AX,0b800h
	mov	DH,DL
	mov	DI,BP
	call	DISP
	mov	AX,0e000h
	mov	DH,DL
	mov	DI,BP
	call	DISP

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN

DISP:
	mov	ES,AX
	EVEN
next:
	lodsw
	xchg	AH,AL
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	shl	AX,1
	shl	AX,1
	rcl	BX,1
	mov	AL,BL
	stosb
	dec	CH
	jnz	next
	add	DI,80		;dummy
next_line	EQU	$-1
	add	SI,CX
	add	SI,CX
	mov	CH,CL
	dec	DH
	jnz	next
	retn
endfunc

END
