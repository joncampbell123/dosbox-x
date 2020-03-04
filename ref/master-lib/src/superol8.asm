; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	void isuper_roll_put_8( int x, int y, int num ) ;
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
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc
;
;$Id: superol8.asm 0.08 92/05/29 20:31:54 Kazumi Rel $
;
	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

func SUPER_ROLL_PUT_8
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	DI,[BP+y]
	mov	SI,DI
	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	CX,[BP+x]
	shr	CX,3		;CX=x/8
	add	DI,CX		;GVRAM offset address
	mov	WORD PTR CS:[_DI_],DI
	mov	AX,[BP+num]
	shl	AX,1		;integer size & near pointer
	mov	BX,OFFSET super_patsize
	add	BX,AX
	mov	DX,[BX]		;pattern size (1-8)
	mov	BX,DX
	xor	BH,BH
	add	BX,SI		;y + size * 8
	sub	BX,400
	jg	short skip
	xor	BX,BX
skip:
	mov	WORD PTR CS:[_ROLLUP_],BX
	mov	CL,DH
	xor	CH,CH
	sub	DL,BL
	mov	DH,DL
	mov	BX,OFFSET super_patdata
	add	BX,AX
	xor	SI,SI
	mov	AX,[BX]		;BX+2 -> BX
	mov	DS,AX
	mov	AX,80
	sub	AX,CX
	mov	BYTE PTR CS:[next_line1],AL
	mov	BYTE PTR CS:[next_line2],AL
	mov	BYTE PTR CS:[next_line3],AL
	mov	BYTE PTR CS:[next_line4],AL
	mov	BX,CX
	mov	AX,0a800h
	mov	ES,AX
	mov	AX,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP		;cls
	mov	DH,DL
	mov	AX,0ff00h + 11001110b
	out	7ch,AL		;RMW mode
	mov	AL,AH		;AL==0ffh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	call	DISP
	mov	DH,DL
	mov	AL,11001101b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	DH,DL
	mov	AL,11001011b
	out	7ch,AL		;RMW mode
	call	DISP
	mov	DH,DL
	mov	AL,11000111b
	out	7ch,AL		;RMW mode
	call	DISP
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN

DISP:
	mov	BP,1111h	;dummy
_ROLLUP_	equ	$-2
	mov	DI,1111h	;dummy
_DI_		equ	$-2
	test	DI,1
	jnz	short odd_address
	test	CX,1
	jnz	short eaos
	EVEN
eaes:
	shr	CX,1
	rep	movsw
	add	DI,80		;dummy
next_line1	equ	$-1
	mov	CX,BX
	dec	DH
	jnz	eaes
	or	BP,BP
	jnz	short roll1
	retn
	EVEN
roll1:
	sub	DI,7d00h
	mov	AX,BP
	mov	DH,AL
	xor	BP,BP
	jmp	eaes
	EVEN
eaos:
	shr	CX,1
	rep	movsw
	movsb
	add	DI,80		;dummy
next_line2	equ	$-1
	mov	CX,BX
	dec	DH
	jnz	eaos
	or	BP,BP
	jnz	short roll2
	retn
	EVEN
roll2:
	sub	DI,7d00h
	mov	AX,BP
	mov	DH,AL
	xor	BP,BP
	jmp	eaos
	EVEN
odd_address:
	test	CX,1
	jnz	short oaos
	EVEN
oaes:
	shr	CX,1
	dec	CX
	movsb
	rep	movsw
	movsb
	add	DI,80		;dummy
next_line3	equ	$-1
	mov	CX,BX
	dec	DH
	jnz	oaes
	or	BP,BP
	jnz	short roll3
	retn
	EVEN
roll3:
	sub	DI,7d00h
	mov	AX,BP
	mov	DH,AL
	xor	BP,BP
	jmp	oaes
	EVEN
oaos:
	shr	CX,1
	movsb
	rep	movsw
	add	DI,80		;dummy
next_line4	equ	$-1
	mov	CX,BX
	dec	DH
	jnz	oaos
	or	BP,BP
	jnz	short roll4
	retn
	EVEN
roll4:
	sub	DI,7d00h
	mov	AX,BP
	mov	DH,AL
	xor	BP,BP
	jmp	oaos
endfunc

END
