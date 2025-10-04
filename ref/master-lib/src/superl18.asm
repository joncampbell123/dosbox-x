; superimpose & master library module
;
; Description:
;	パターンの表示(画面上下接続, 特定プレーン, 横8dot単位)
;
; Functions/Procedures:
;	void super_roll_put_1plane_8( int x, int y, int num, int pat_plane, int put_plane ) ;
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
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superl18.asm 0.02 92/05/29 20:28:36 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	94/ 1/ 3 [M0.22] 自己書換を撲滅
;

	.186
	.MODEL SMALL
	include func.inc
	.DATA

	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

func SUPER_ROLL_PUT_1PLANE_8	; {
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

	CLD
	mov	DI,[BP+y]
	mov	CX,[BP+pat_plane]
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
	mov	BX,[BP+x]
	shr	BX,3		;BX=x/8
	add	DI,BX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	DS,super_patdata[BX]
	mov	BP,AX
	mov	AL,DH
	mul	DL		; AX = パターンの 1planeのバイト数
	xor	SI,SI
	jcxz	short plane_end
plane_search:
	add	SI,AX
	loop	short plane_search
plane_end:
	mov	AX,DX
	xor	AH,AH
	add	AX,BP		;y + size * 8
	sub	AX,400
	jg	short skip
	xor	AX,AX
skip:
	mov	BL,DH
	xor	BH,BH
	sub	DL,AL
	mov	DH,DL
	mov	BP,80
	sub	BP,BX
	mov	CX,0a800h
	mov	ES,CX

	test	DI,1
	jnz	short odd_address
	shr	BX,1
	jc	short eaos
	EVEN
eaes:
	mov	CX,BX
	rep	movsw
	add	DI,BP
	dec	DH
	jnz	short eaes
	or	AX,AX
	jz	short return
roll1:
	sub	DI,7d00h
	mov	DH,AL
	xor	AX,AX
	jmp	short eaes
	EVEN
eaos:
	mov	CX,BX
	rep	movsw
	movsb
	add	DI,BP
	dec	DH
	jnz	short eaos
	or	AX,AX
	jz	short return

	sub	DI,7d00h
	mov	DH,AL
	xor	AX,AX
	jmp	short eaos
	EVEN
odd_address:
	shr	BX,1
	jc	short oaos
	dec	BX
	EVEN
oaes:
	mov	CX,BX
	movsb
	rep	movsw
	movsb
	add	DI,BP
	dec	DH
	jnz	short oaes
	or	AX,AX
	jz	short return

	sub	DI,7d00h
	mov	DH,AL
	xor	AX,AX
	jmp	short oaes
	EVEN
oaos:
	mov	CX,BX
	movsb
	rep	movsw
	add	DI,BP
	dec	DH
	jnz	short oaos
	or	AX,AX
	jz	short return

	sub	DI,7d00h
	mov	DH,AL
	xor	AX,AX
	jmp	short oaos
	EVEN
return:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	10
endfunc			; }

END
