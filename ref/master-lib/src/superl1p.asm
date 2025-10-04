; superimpose & master library module
;
; Description:
;	ÉpÉ^Å[ÉìÇÃï\é¶(âÊñ è„â∫ê⁄ë±, ì¡íËÉvÉåÅ[Éì)
;
; Functions/Procedures:
;	void super_roll_put_1plane( int x, int y, int num, int pat_plane, unsigned put_plnae ) ;
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
;	PC-9801V
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
;
;$Id: superl1p.asm 0.02 92/05/29 20:29:24 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] WORD_MASKîpé~
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	BYTE_MASK:BYTE

	.CODE

func SUPER_ROLL_PUT_1PLANE	; super_roll_put_1plane() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+5)*2
	y	= (RETSIZE+4)*2
	num	= (RETSIZE+3)*2
	pat_plane = (RETSIZE+2)*2
	put_plane = (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	AX,[BP+put_plane]
	out	7ch,AL		;RMW mode
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	AX,DI		;-+
	shl	DI,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	BX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	BX,3		;BX=x/8
	add	DI,BX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	push	BX
	mov	BX,CX
	mov	CX,[BP+pat_plane]
	mov	BP,AX
	mov	AL,DH
	mul	DL
	xor	SI,SI
	jcxz	short plane_end
plane_search:
	add	SI,AX
	loop	short plane_search
plane_end:
	mov	AL,BYTE_MASK[BX]
	mov	CH,DH		;DL -> DH
	shr	CH,1
	mov	CL,BL
	pop	BX
	mov	DS,super_patdata[BX]
	mov	BX,0a800h
	mov	ES,BX
	mov	BX,DX
	xor	BH,BH
	add	BP,BX
	sub	BP,400
	jg	short noroll
	xor	BP,BP
noroll:
	sub	BX,BP

	test	DI,1
	jz	short even_address
	jmp	odd_address
even_address:
	xor	DL,DL
	test	DH,1		;DX -> DH
	jnz	short odd_size1
	mov	BYTE PTR CS:[word_mask1],AL
	mov	BYTE PTR CS:[count1],CH
	mov	AL,80
	sub	AL,DH		;DL -> DH
	mov	BYTE PTR CS:[add_di1],AL
	EVEN
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask1	equ	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop1
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di1		equ	$-1
	mov	CH,11h	;dummy
count1		equ	$-1
	dec	BX
	jnz	short put_loop1
	or	BP,BP
	jnz	short roll1
	jmp	return
	EVEN
roll1:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop1
	EVEN
odd_size1:
	mov	BYTE PTR CS:[word_mask2],AL
	mov	BYTE PTR CS:[count2],CH
	mov	AL,80
	sub	AL,DH
	mov	BYTE PTR CS:[add_di2],AL
	EVEN
single_check2:
	or	CH,CH
	jz	short skip2
	EVEN
put_loop2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask2	equ	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	dec	DI
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di2		equ	$-1
	mov	CH,11h	;dummy
count2		equ	$-1
	dec	BX
	jnz	short single_check2
	or	BP,BP
	jnz	short roll2
	jmp	return
	EVEN
roll2:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	single_check2
	EVEN
odd_address:
	dec	DI
	test	DH,1
	jnz	short odd_size2
even_size2:
	mov	BYTE PTR CS:[word_mask3],AL
	dec	CH		;!!!!!!!!!!!!!!!!!!!!!
	mov	BYTE PTR CS:[count3],CH
	mov	AL,78		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di3],AL
	EVEN
put_loop3_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short no_word1
	EVEN
put_loop3_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask3	equ	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop3_2
	EVEN
no_word1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di3		equ	$-1
	mov	CH,11h	;dummy
count3		equ	$-1
	dec	BX
	jnz	short put_loop3_1
	or	BP,BP
	jz	short return
roll3:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop3_1
	EVEN
odd_size2:
	mov	BYTE PTR CS:[word_mask4],AL
	mov	BYTE PTR CS:[count4],CH
	mov	AL,79		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di4],AL
	EVEN
put_loop4_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short no_word2
	EVEN
put_loop4_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask4	equ	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short put_loop4_2
	EVEN
no_word2:
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di4		equ	$-1
	mov	CH,11h	;dummy
count4		equ	$-1
	dec	BX
	jnz	short put_loop4_1
	or	BP,BP
	jz	short return
roll4:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop4_1
	EVEN
return:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	10
endfunc				; }

END
