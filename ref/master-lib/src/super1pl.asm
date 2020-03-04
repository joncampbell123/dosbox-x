; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	super_put_1plane
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
;
;$Id: super1pl.asm 0.03 92/05/29 20:19:10 Kazumi Rel $
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

func SUPER_PUT_1PLANE
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
	mov	SI,[BP+pat_plane]
	mov	AX,[BP+put_plane]
	out	7ch,AL		;RMW mode
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)
	push	BX
	mov	AL,DH
	xor	AH,AH
	mul	DL
	mov	BP,AX
	pop	AX
	xor	BX,BX
	or	SI,SI
	jz	short plane_end
plane_search:
	add	BX,BP
	dec	SI
	jnz	plane_search
plane_end:
	mov	ES,BX
	mov	SI,CX
	mov	BL,BYTE_MASK[SI]
	mov	CH,DH		;DL -> DH
	shr	CH,1
	test	DI,1
	jz	short even_address
	jmp	odd_address
even_address:
	mov	BP,DI		;save DI
	test	DH,1		;DX -> DH
	jnz	short odd_size1
	mov	BYTE PTR CS:[word_mask1],BL
	mov	BYTE PTR CS:[count1],CH
	mov	BX,AX
	mov	SI,ES
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	mov	AL,80
	sub	AL,DH		;DL -> DH
	mov	BYTE PTR CS:[add_di1],AL
	mov	AX,0a800h
	mov	ES,AX
	xor	DL,DL
	EVEN
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
word_mask1	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	put_loop1
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di1		EQU	$-1
	mov	CH,11h	;dummy
count1		EQU	$-1
	dec	BX
	jnz	put_loop1
	jmp	return
	EVEN
odd_size1:
	mov	BYTE PTR CS:[word_mask2],BL
	mov	BYTE PTR CS:[count2],CH
	mov	BX,AX
	mov	SI,ES
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	mov	AL,80
	sub	AL,DH
	mov	BYTE PTR CS:[add_di2],AL
	mov	AX,0a800h
	mov	ES,AX
	xor	DL,DL
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
word_mask2	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	put_loop2
skip2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	dec	DI
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di2		EQU	$-1
	mov	CH,11h	;dummy
count2		EQU	$-1
	dec	BX
	jnz	single_check2
	jmp	return
	EVEN

odd_address:
	dec	DI
	mov	BP,DI		;save DI
	test	DH,1

	jnz	short odd_size2
	mov	BYTE PTR CS:[word_mask3],BL
	dec	CH		;!!!!!!!!!!!!!!!!!!!!!
	mov	BYTE PTR CS:[count3],CH
	mov	BX,AX
	mov	SI,ES
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	mov	AL,78		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di3],AL
	mov	AX,0a800h
	mov	ES,AX
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
word_mask3	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	put_loop3_2
	EVEN
no_word1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di3		EQU	$-1
	mov	CH,11h	;dummy
count3		EQU	$-1
	dec	BX
	jnz	put_loop3_1
	jmp	short return
	EVEN

odd_size2:
	mov	BYTE PTR CS:[word_mask4],BL
	mov	BYTE PTR CS:[count4],CH
	mov	BX,AX
	mov	SI,ES
	mov	DS,super_patdata[BX]
	mov	BX,DX
	xor	BH,BH
	mov	AL,79		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di4],AL
	mov	AX,0a800h
	mov	ES,AX
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
word_mask4	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	put_loop4_2
	EVEN
no_word2:
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di4		EQU	$-1
	mov	CH,11h	;dummy
count4		EQU	$-1
	dec	BX
	jnz	put_loop4_1
return:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	10
endfunc

END
