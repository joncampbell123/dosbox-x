; superimpose & master library module
;
; Description:
;	パターンの表示(画面上下接続)
;
; Functions/Procedures:
;	void super_roll_put( int x, int y, int num ) ;
;
; Parameters:
;	x,y	描画する座標
;	num	パターン番号
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
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superoll.asm 0.05 92/05/29 20:33:19 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] WORD_MASK廃止, サイズ数百バイト減少
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	BYTE_MASK:BYTE

	.CODE

func SUPER_ROLL_PUT
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
	mov	SI,DI		;-+
	shl	DI,2		; |
	add	DI,SI		; |DI=y*80
	shl	DI,4		;-+
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address

	mov	BX,CX
	mov	AL,BYTE_MASK[BX]

	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	CH,DH		;DL -> DH
	shr	CH,1

	mov	DS,super_patdata[BX]		;BX+2 -> BX
	mov	BX,DX
	xor	BH,BH
	add	SI,BX		; SI = y + ysize - 400
	sub	SI,400
	jg	short noroll
	xor	SI,SI		; SI = 画面下にはみ出た長さ
noroll:
	sub	BX,SI		; BX = 画面内に描画できる長さ

	mov	CS:_BP_,SI
	mov	CS:_BX_,BX

	mov	BP,DI

	test	DI,1
	jnz	short ODD_ADDRESS

EVEN_ADDRESS:
	test	DH,1		;DX -> DH
	jnz	short ODD_SIZE1

	mov	BYTE PTR CS:[BYTE_MASK1],AL
	mov	BYTE PTR CS:[count1],CH
	mov	AL,80
	sub	AL,DH		;DL -> DH
	mov	BYTE PTR CS:[add_di1],AL
	mov	CS:DISP_ADDRESS,OFFSET disp1
	jmp	short START
	EVEN

ODD_SIZE1:
	mov	BYTE PTR CS:[BYTE_MASK2],AL
	mov	BYTE PTR CS:[count2],CH
	mov	AL,80
	sub	AL,DH
	mov	BYTE PTR CS:[add_di2],AL
	mov	CS:DISP_ADDRESS,OFFSET disp2
	jmp	short START
	EVEN

ODD_ADDRESS:
	dec	BP
	test	DH,1
	jnz	short ODD_SIZE2

	mov	BYTE PTR CS:[BYTE_MASK3],AL
	dec	CH		;!!!!!!!!!!!!!!!!!!!!!
	mov	BYTE PTR CS:[count3],CH
	mov	AL,78		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di3],AL
	mov	CS:DISP_ADDRESS,OFFSET disp3
	jmp	short START
	EVEN

ODD_SIZE2:
	mov	BYTE PTR CS:[BYTE_MASK4],AL
	mov	BYTE PTR CS:[count4],CH
	mov	AL,79		;word
	sub	AL,DH
	mov	BYTE PTR CS:[add_di4],AL
	mov	CS:DISP_ADDRESS,OFFSET disp4
	EVEN

START:
	mov	CS:_DI_,BP
	xor	SI,SI

	mov	AX,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,AH
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	mov	AX,0a800h
	mov	ES,AX
	call	DISP		;originally cls_loop
	mov	AX,0ff00h + 11001110b
	out	7ch,AL		;RMW mode
	mov	AL,AH		;AL==0ffh
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
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
	EVEN

DISP:
	JMOV	BX,_BX_
	JMOV	DI,_DI_
	JMOV	BP,_BP_
	JMOV	AX,DISP_ADDRESS
	jmp	AX
	EVEN
disp1:
	xor	DL,DL
	EVEN
put_loop1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
BYTE_MASK1	equ	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	put_loop1
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
add_di1		equ	$-1
	mov	CH,11h	;dummy
count1		equ	$-1
	dec	BX
	jnz	put_loop1
	or	BP,BP
	jnz	short roll1
	retn
	EVEN
roll1:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop1
	EVEN
disp2:
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
BYTE_MASK2	equ	$-1
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
add_di2		equ	$-1
	mov	CH,11h	;dummy
count2		equ	$-1
	dec	BX
	jnz	single_check2
	or	BP,BP
	jnz	short roll2
	retn
	EVEN
roll2:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short single_check2
	EVEN
disp3:
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
BYTE_MASK3	equ	$-1
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
add_di3		equ	$-1
	mov	CH,11h	;dummy
count3		equ	$-1
	dec	BX
	jnz	put_loop3_1
	or	BP,BP
	jnz	short roll3
	retn
	EVEN
roll3:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop3_1
	EVEN
disp4:
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
BYTE_MASK4	equ	$-1
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
add_di4		equ	$-1
	mov	CH,11h	;dummy
count4		equ	$-1
	dec	BX
	jnz	put_loop4_1
	or	BP,BP
	jnz	short roll4
	retn
	EVEN
roll4:
	sub	DI,7d00h
	mov	BX,BP
	xor	BP,BP
	jmp	short put_loop4_1
endfunc

END
