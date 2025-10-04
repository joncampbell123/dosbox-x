; superimpose & master library module
;
; Description:
;	パターンの表示(特定プレーン, 横8dot単位)
;
; Functions/Procedures:
;	void super_put_1plane_8( int x, int y, int num, int pat_palne, unsigned put_plane ) ;
;
; Parameters:
;	x,y	描画する座標
;	num	パターン番号
;	pat_plane 0～3. 描画するパターンデータのプレーン番号
;	put_plane 描画先のプレーン指定。下位がGRCGのモードワード。
;		  上位がパターンレジスタに設定する値。
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
;$Id: super1p8.asm 0.04 92/05/29 20:18:30 Kazumi Rel $
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	94/ 1/ 3 [M0.22] ついでにちょっと縮小+加速
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

func SUPER_PUT_1PLANE_8		; super_put_1plane_8() {
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
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	shr	CX,3		;CX=x/8
	add	DI,CX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]
	mov	DS,super_patdata[BX]
	mov	CX,[BP+pat_plane]
	mov	AL,DH
	mul	DL
	xor	SI,SI
	jcxz	short plane_end
plane_search:
	add	SI,AX
	loop	short plane_search
plane_end:
	mov	BL,DH
	mov	BH,0
	mov	DH,BH	; 0
	mov	AX,0a800h
	mov	ES,AX
	mov	AX,80
	sub	AX,BX
disp:
	test	DI,1
	jnz	short odd_address
	shr	BX,1
	jc	short eaos
	EVEN
eaes:
	mov	CX,BX
	rep	movsw
	add	DI,AX
	dec	DX
	jnz	short eaes
	jmp	short return
	EVEN
eaos:
	mov	CX,BX
	rep	movsw
	movsb
	add	DI,AX
	dec	DX
	jnz	short eaos
	jmp	short return
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
	add	DI,AX
	dec	DX
	jnz	short oaes
	jmp	short return
	EVEN
oaos:
	mov	CX,BX
	movsb
	rep	movsw
	add	DI,AX
	dec	DX
	jnz	short oaos
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
