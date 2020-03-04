; superimpose & master library module
;
; Description:
;	キャラクタを表示する
;
; Functions/Procedures:
;	void super_in(int x,int y,int charnum) ;
;
; Parameters:
;	x,y	座標
;	charnum	キャラクタ番号
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
;$Id: superin.asm 0.06 92/05/29 20:27:26 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/20 [M0.21] 32byte程度(笑)縮小
;	93/ 9/20 [M0.21] WORD_MASK廃止
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	super_charnum:WORD, super_chardata:WORD

	EXTRN	BYTE_MASK:BYTE

	.CODE

func SUPER_IN		; {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	charnum	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+charnum]

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+

	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	shl	BX,1		;integer size & near pointer
	mov	BP,BX		;save BX
	mov	BX,super_charnum[BX]	;pattern number
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)

	push	DI		;この辺はスゲー手抜き(笑)
	push	BX
	push	CX
	push	DS
	mov	SI,DI
	xor	DI,DI
	mov	ES,DS:super_chardata[BP]		;BX+2 -> BX
	mov	AL,DH
	xor	AH,AH
	shr	AX,1
	inc	AX
	mov	CS:BACKUP_CX,AX
	shl	AX,1
	mov	BX,80
	sub	BX,AX
	mov	CS:BACKUP_ADD,BX
	mov	AX,DX
	xor	AH,AH
	mov	CS:BACKUP_BX,AX
	mov	BP,SI
	mov	AX,0a800h
	call	BACKUP
	mov	SI,BP
	mov	AX,0b000h
	call	BACKUP
	mov	SI,BP
	mov	AX,0b800h
	call	BACKUP
	mov	SI,BP
	mov	AX,0e000h
	call	BACKUP
	pop	DS
	pop	CX
	pop	BX
	pop	DI

	mov	AX,DX
	xor	AH,AH
	mov	CS:_BX_,AX

	mov	SI,CX
	mov	CH,DH
	shr	CH,1
	mov	AL,BYTE_MASK[SI]

	test	DI,1
	jnz	short ODD_ADDRESS

EVEN_ADDRESS:
	test	DH,1
	jnz	short ODD_SIZE1

	mov	BYTE PTR CS:[WORD_MASK1],AL
	mov	BYTE PTR CS:[COUNT1],CH
	mov	AL,80
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI1],AL
	mov	CS:DISP_ADDRESS,offset DISP1 - offset JUMP_ORIGIN
	jmp	short START

	EVEN
ODD_SIZE1:
	mov	BYTE PTR CS:[WORD_MASK2],AL
	mov	BYTE PTR CS:[COUNT2],CH
	mov	AL,80
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI2],AL
	mov	CS:DISP_ADDRESS,offset DISP2 - offset JUMP_ORIGIN
	jmp	short START

	EVEN
ODD_ADDRESS:
	dec	DI
	test	DH,1
	jnz	short ODD_SIZE2

	mov	BYTE PTR CS:[WORD_MASK3],AL
	dec	CH		;!!!!!!!!!!!!!!!!!!!!!
	mov	BYTE PTR CS:[COUNT3],CH
	mov	AL,78		;word
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI3],AL
	mov	CS:DISP_ADDRESS,offset DISP3 - offset JUMP_ORIGIN
	jmp	short START

	EVEN
ODD_SIZE2:
	mov	BYTE PTR CS:[WORD_MASK4],AL
	mov	BYTE PTR CS:[COUNT4],CH
	mov	AL,79		;word
	sub	AL,DH
	mov	BYTE PTR CS:[ADD_DI4],AL
	mov	CS:DISP_ADDRESS,offset DISP4 - offset JUMP_ORIGIN

	EVEN
START:
	mov	DS,super_patdata[BX]
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
	mov	BP,DI		;save DI
	call	DISP		;originally cls_loop
	mov	AX,0ff00h + 11001110b
	out	7ch,AL		;RMW mode
	mov	AL,AH		;AL==0ffh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	DI,BP
	call	DISP
	mov	AL,11001101b
	out	7ch,AL		;RMW mode
	mov	DI,BP
	call	DISP
	mov	AL,11001011b
	out	7ch,AL		;RMW mode
	mov	DI,BP
	call	DISP
	mov	AL,11000111b
	out	7ch,AL		;RMW mode
	mov	DI,BP
	call	DISP

	xor	AL,AL
	out	7ch,AL		;grcg off

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc			; }

;
; 待避
;
	EVEN
BACKUP	proc	near
	mov	DS,AX
	JMOV	BX,BACKUP_BX
	JMOV	AX,BACKUP_ADD

	EVEN
BACKUP_LOOP:
	JMOV	CX,BACKUP_CX
	rep	movsw
	add	SI,AX
	dec	BX
	jnz	short BACKUP_LOOP
	ret
BACKUP	endp

;
; 表示
;

	EVEN
DISP1	proc	near
	xor	DL,DL
	EVEN
PUT_LOOP1:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
WORD_MASK1	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP1
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI1		EQU	$-1
	mov	CH,11h	;dummy
COUNT1		EQU	$-1
	dec	BX
	jnz	short PUT_LOOP1
	ret
DISP1	endp


	EVEN
DISP2	proc	near
	xor	DL,DL
	EVEN
SINGLE_CHECK2:
	or	CH,CH
	jz	short SKIP2
	EVEN
PUT_LOOP2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
WORD_MASK2	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP2
SKIP2:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	dec	DI
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI2		EQU	$-1
	mov	CH,11h	;dummy
COUNT2		EQU	$-1
	dec	BX
	jnz	short SINGLE_CHECK2
	ret
DISP2	endp

	EVEN
DISP3	proc	near
PUT_LOOP3_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short NO_WORD1
	EVEN
PUT_LOOP3_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
WORD_MASK3	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP3_2
	EVEN
NO_WORD1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	or	AL,DL
	stosw
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI3		EQU	$-1
	mov	CH,11h	;dummy
COUNT3		EQU	$-1
	dec	BX
	jnz	short PUT_LOOP3_1
	ret
DISP3	endp

	EVEN
DISP4	proc	near
PUT_LOOP4_1:
	lodsb
	xor	AH,AH
	ror	AX,CL
	mov	DL,AH
	xchg	AH,AL
	xor	AL,AL
	stosw
	or	CH,CH
	jz	short NO_WORD2
	EVEN
PUT_LOOP4_2:
	lodsw
	ror	AX,CL
	mov	DH,AL
	and	AL,11h	;dummy
WORD_MASK4	EQU	$-1
	xor	DH,AL
	or	AL,DL
	stosw	;mov 	ES:[DI],AX	;;;or
	mov	DL,DH
	dec	CH
	jnz	short PUT_LOOP4_2
	EVEN
NO_WORD2:
	mov	ES:[DI],DL
	mov	DL,CH	;DL=0
	add	DI,80	;dummy
ADD_DI4		EQU	$-1
	mov	CH,11h	;dummy
COUNT4		EQU	$-1
	dec	BX
	jnz	short PUT_LOOP4_1
	ret
DISP4	endp

;
; 表示エントリ
;
DISP	proc	near
	JMOV	BX,_BX_
	jmp	near ptr DISP1	; 遠いところをダミーにね
	org	$-2
DISP_ADDRESS	dw	?
JUMP_ORIGIN	label near
DISP	endp

END
