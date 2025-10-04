; superimpose & master library module
;
; Description:
;	大きさを縦横2倍に拡大してパターンを表示する
;
; Functions/Procedures:
;	void super_large_put( int x, int y, int num ) ;
;
; Parameters:
;	x,y	左上端の座標
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
;	CPU: 8086
;
; Notes:
;	クリッピングしてません
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
;$Id: largeput.asm 0.04 92/05/29 20:06:02 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	graph_VramSeg:WORD

	.CODE

	EXTRN	LARGE_BYTE:BYTE		; code segment!

func SUPER_LARGE_PUT		; super_large_put() {
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	num	= (RETSIZE+1)*2

	mov	ES,graph_VramSeg

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	WORD PTR CS:[_DI_],DI

	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	xor	SI,SI
	mov	DS,super_patdata[BX]	;BX+2 -> BX
	mov	BX,DX
	xor	BH,BH
	mov	DL,DH		;????
	xor	DH,DH		;????
	mov	BYTE PTR CS:[_CH_],BL
	mov	AX,160
	sub	AX,DX
	sub	AX,DX		;large
	mov	WORD PTR CS:[add_di],AX

	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	xor	AL,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	DH,DL
	call	DISP8		;originally cls_loop

	mov	AL,0FFh
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL

	mov	AL,11001110b
	out	7ch,AL		;RMW mode
	call	DISP8
	mov	AL,11001101b
	out	7ch,AL		;RMW mode
	call	DISP8
	mov	AL,11001011b
	out	7ch,AL		;RMW mode
	call	DISP8
	mov	AL,11000111b
	out	7ch,AL		;RMW mode
	call	DISP8
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc				; }

	; IN:
	;	AH
	;	DH
	
DISP8	proc near
	mov	CH,11h		;dummy
_CH_	EQU	$-1
	mov	DI,1111h	;dummy
_DI_	EQU	$-2
	EVEN

PUT_LOOP:
	lodsb
	mov	BP,AX
	and	AX,00f0h
	shr	AX,4
	mov	BX,AX
	mov	AL,CS:LARGE_BYTE[BX]
	xor	AH,AH
	ror	AX,CL
	mov	ES:[DI],AX	;stosw より速い
	mov	ES:[DI+80],AX
	inc	DI
	and	BP,000fh
	mov	AL,CS:LARGE_BYTE[BP]
	xor	AH,AH
	ror	AX,CL
	mov	ES:[DI],AX	;stosw より速い
	mov	ES:[DI+80],AX
	inc	DI
	dec	DH
	jnz	short PUT_LOOP

	add	DI,1111h	;dummy
add_di	EQU	$-2
	mov	DH,DL
	dec	CH
	jnz	short PUT_LOOP

	ret
DISP8	endp

END
