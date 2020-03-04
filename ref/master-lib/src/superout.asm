; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	void super_out( int x, int y, int num ) ;
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
;	GRCG
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
;
;$Id: superout.asm 0.07 92/05/29 20:34:18 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	super_charnum:WORD, super_chardata:WORD

	.CODE

buffer		DB	2304 dup (?)
	EVEN

func SUPER_OUT		; {
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
	mov	CX,[BP+num]
	mov	DI,AX		;-+
	shl	DI,2		; |SI=y*80
	add	DI,AX		; |
	shl	DI,4		;-+
	mov	AX,DX
	and	AX,7
	mov	BYTE PTR CS:[bit_shift],AL
	shr	DX,3		;AX=x/8
	add	DI,DX		;GVRAM offset address
	mov	BP,DI
	shl	CX,1		;integer size & near pointer
	mov	BX,CX
	mov	AX,super_charnum[BX]
	shl	AX,1		;integer size & near pointer
	mov	BX,AX
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	push	DS
	push	CX
	mov	BX,AX
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
	and	AX,1
	jz	short even_size
	movsb
even_size:
	pop	CX
	pop	DS
	mov	DI,BP
	mov	BX,CX
	xor	SI,SI
	mov	DS,super_chardata[BX]		;BX+2 -> BX
	mov	CH,DH
	mov	BYTE PTR CS:[xcount],CH
	mov	AL,80
	sub	AL,CH
	mov	BYTE PTR CS:[next_line],AL
	mov	AH,1
	mov	AL,DH
	and	AL,1
	jnz	short no_inc
	inc	AH
no_inc:
	mov	BYTE PTR CS:[skip_byte],AH
	mov	CL,11h
bit_shift	EQU	$-1
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

	EVEN

REPAIR:
	mov	ES,AX
	mov	BX,OFFSET buffer
	EVEN
repair_loop:
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
	jnz	repair_loop
	add	DI,80		;dummy
next_line	EQU	$-1
	add	SI,0		;dummy
skip_byte	EQU	$-1
	mov	CH,11h
xcount		EQU	$-1
	dec	DH
	jnz	repair_loop
	retn
endfunc			; }

END
