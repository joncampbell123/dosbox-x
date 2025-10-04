; master library - grpahics - xor - boxfill - PC98V
;
; Description:
;	グラフィック画面の XOR による箱塗り
;
; Function/Procedures:
;	void graph_xorboxfill( int x1, int y1, int x2, int y2, int color ) ;
;
; Parameters:
;	x1,y1	左上頂点座標
;	x2,y2	右下頂点座標
;	color	画面上の領域に対して xor するカラーコード
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-98V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 1/27 Initial: grpxorbf.asm
;	93/ 6/ 7 [M0.19] x座標が両方マイナスだったときのbug fix
;	93/ 9/ 1 [M0.21] 座標範囲のマジックナンバーを擬似変数にいれた
;	93/10/23 [M0.21] graph_VramSeg,graph_VramLinesを使用

	.186
	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN	graph_VramSeg:WORD
	EXTRN	graph_VramLines:WORD

	.CODE

XDOT	= 640
XBYTES	= (XDOT/8)

MRETURN macro
	pop	DI
	pop	SI
	pop	BP
	ret	5*2
	EVEN
	endm

retfunc OWARI
	MRETURN
endfunc

; initial 90/6/7
func GRAPH_XORBOXFILL
	push	BP
	mov	BP,SP

	; 引数
	x1 = (RETSIZE+5)*2
	y1 = (RETSIZE+4)*2
	x2 = (RETSIZE+3)*2
	y2 = (RETSIZE+2)*2
	color = (RETSIZE+1)*2

	push	SI
	push	DI

	;************************** view clipping
	mov	AX,[BP+x1]	; まず、x座標のクリップ**********
	mov	BX,[BP+x2]
	test	AX,BX
	js	short OWARI	; 両方マイナスなら終わり

	cmp	AX,BX
	jle	short Lskip1	; if ( AX > BX )
		xchg	AX,BX	;	swap AX,BX;
	Lskip1:
	cmp	AX,8000h	; if ( AX < 0 )
	sbb	DX,DX		;	AX = 0
	and	AX,DX

	sub	BX,(XDOT-1)		; if ( BX >= XDOT )
	sbb	DX,DX		;	BX = (XDOT-1) ;
	and	BX,DX
	add	BX,(XDOT-1)

	cmp	AX,BX		; if ( AX > BX )
	jg	short OWARI	;	goto OWARI;


	mov	CX,AX		; CX = x(L) & 15,
	and	CX,0fh		;
	mov	SI,AX		; SI = x(L) / 16 * 2,
	sar	SI,3		;
	and	SI,3feh		;
	sub	BX,AX		; DX = x(H)-x(L)+1.
	inc	BX		;
	mov	DX,BX		;

	mov	AX,[BP+y1]	; 次に、y座標のクリップ************
	mov	BX,[BP+y2]
	cmp	AX,BX
	jle	short Lskip4	; if ( AX > BX )
	xchg	AX,BX		;	swap AX,BX;
Lskip4:
	test	AX,AX
	jns	short Lskip5	; if ( AX < 0 )
	xor	AX,AX		;	AX = 0;
Lskip5:
	mov	DI,graph_VramLines
	cmp	BX,DI
	jl	short Lskip6	; if ( BX >= YDOT )
	lea	BX,[DI-1]	;	BX = YDOT - 1 ;
Lskip6:
	cmp	AX,BX		; if ( AX > BX )
	jg	short OWARI	;	goto OWARI;

	push	CX
	push	DX
	mov	CX,XBYTES
	sub	BX,AX
	mul	CX
	add	SI,AX		; SI += (BX-AX)* XBYTES
	mov	AX,BX
	mul	CX
	mov	DI,AX		; DI = BX * XBYTES
	pop	DX
	pop	CX

;   現在の状態	    CL..ｽﾀｰﾄ点のドットアドレス
;		    SI..ｽﾀｰﾄ点のワードアドレス
;		    DX..ｴﾝﾄﾞ点までの横ドット数
;		    DI..ｴﾝﾄﾞ点までの縦アドレス差

;************************** draw boxfill ***************************

	mov	BX,[BP+color]
	mov	CH,BL
	mov	AX,graph_VramSeg
	push	DS
PLANE_START:
	shr	CH,1
	jnb	short Lplane_c
	push	SI
	push	DI
	mov	DS,AX
	mov	ES,CX
	mov	BP,DX

	mov	BX,8000h	; 最初のドット生成
	shr	BX,CL		;
	mov	AX,BX		;
	mov	CX,DX
Lloop2:				; 最初のワード生成
	or	AX,BX		;
	dec	CX		;
	je	short Llast	;
	shr	BX,1		;
	jnb	short Lloop2	;
	mov	BX,DI
	xchg	AH,AL
Lloop3:				; 最初の横１ワード＊縦全部
	xor	[BX+SI],AX	;
	sub	BX,XBYTES	;
	jns	Lloop3		;
	inc	SI
	inc	SI
	cmp	CX,16		; 残りドット数が15以下なら
	jb	short Llastword	;	Llastwordへ
	mov	DX,CX		; DX = 残りドット数 & 15
	and	DX,0fh		;
	sar	CX,4		; CX = 残りドット数 / 16
	mov	AX,XBYTES	;
Lloop4:				; 次の横ｎワード＊縦全部
	mov	BX,DI		;      2
Lloop5:				;
	not	word ptr [BX+SI]; 16+8=24
	sub	BX,AX		;      3
	jns	short Lloop5	; 16/4 -> (27+16)*ys-12
	inc	SI		;      2
	inc	SI		;      2
	loop	short Lloop4	; 17/5 ->
	mov	CL,DL		; CL = DL( CHはloopの後ﾀﾞｶﾗ 0 )
Llastword:
	dec	CL		; 残りドット数が０なら
	js	short Lq	;	Lqへ
	mov	AX,8000h	; 最後のワード生成
	sar	AX,CL		;
Llast:
	mov	BX,DI
	xchg	AH,AL
Lloop6:				; 最後の１ワード＊縦全部
	xor	[BX+SI],AX	;
	sub	BX,XBYTES	;
	jns	short Lloop6	;

Lq:
	pop	DI
	pop	SI
	mov	DX,BP
	mov	CX,ES
	mov	AX,DS
Lplane_c:
	add	AH,8
	or	AH,20h
	cmp	AH,0e8h
	jb	short PLANE_START

	pop	DS
	MRETURN
endfunc

END
