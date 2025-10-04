; gc_poly library - clip - polygon - convex
;
; Description:
;	凸/凹多角形のクリッピング
;
; Funciton/Procedures:
;	int grc_clip_polygon_n( Point * dest, int ndest,
;				const Point * src, int nsrc ) ;
;
; Parameters:
;	Point *dest	クリップ結果格納先
;	int ndest	destに確保してある要素数
;	Point *src	クリップ元の多角形の頂点配列
;	int nsrc	srcに含まれる点の数
;
; Returns:
;	-1: srcのままでＯＫです。
;	0: 全く範囲外です。描画できません。
;	1～: destにデータを格納しました。点数を返します。
;
;	上記３種類、どの場合でも destの内容は変化します。
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
; 	・destには、凸の場合nsrc+4個,凹の場合nsrc*2個以上を確保しておくこと。
;	　実行時に、この ndest 個全てを書き換えます。
;
;	・凹多角形が入力され、クリッピングの結果が複数の島になってしまう場合、
;	　クリップ枠の周囲に沿って細く繋げた形になります。
;
;	・destとsrcの関係は、全く同じ場合と、全く重ならない場合の
;	　どちらかでなくてはいけません。
;	　dest=srcの場合、戻り値に -1 が返る事はありません。
;
;	・src または dest が NULL かどうかの検査は行っていません。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/ 7/11 Initial
;	92/ 7/17 bugfix(y=ClipYT or YBの点を含むクリップの誤り)
;	92/ 7/30 bugfix(FAR版、srcとdestの一致判定にセグメントが抜けていた)
;	92/11/16 bugfix(FAR版、ret <popvalue>の値が間違っていた
;	93/ 5/10 bugfix(FAR版、ひとつセグメントロードが足りなかった(^^;)
;
	.186
	.MODEL SMALL
	.DATA?

	; クリップ枠
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYB:WORD

	.CODE
	include clip.inc
	include func.inc

; IN: x
; OUT: ans
; BREAK: ans, flag
GETOUTCODE_X macro ans,x,xl,xr
	local OUTEND
	mov	ans,LEFT_OUT
	cmp	xl,x
	jg	short OUTEND
	xor	ans,YOKO_OUT
	cmp	x,xr
	jg	short OUTEND
	xor	ans,ans
OUTEND:
	ENDM

; IN: y
; OUT: ans
; BREAK: ans, flag
GETOUTCODE_Y macro ans,y,yt,yb
	local OUTEND
	mov	ans,BOTTOM_OUT
	cmp	y,yb
	jg	short OUTEND
	xor	ans,TATE_OUT
	cmp	y,yt
	jl	short OUTEND
	xor	ans,ans
OUTEND:
	ENDM

MRETURN macro
	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	(2+datasize*2)*2
	EVEN
	endm

if datasize EQ 2
LODE	macro	dest,src
	mov	dest,ES:src
	endm
STOE	macro	dest,src
	mov	ES:dest,src
	endm
ESSRC	macro
	mov	ES,[BP+src+2]
	endm
ESDEST	macro
	mov	ES,[BP+dest+2]
	endm
else
LODE	macro	dest,src
	mov	dest,src
	endm
STOE	macro	dest,src
	mov	dest,src
	endm
ESSRC	macro
	endm
ESDEST	macro
	endm
endif

;---------------------------------------
; int grc_clip_polygon_n( Point * dest, int ndest,
;			const Point * src, int nsrc ) ;
func GRC_CLIP_POLYGON_N
	ENTER	14,0
	push	SI
	push	DI

	; 引数
	dest	= (RETSIZE+3+DATASIZE)*2
	ndest	= (RETSIZE+2+DATASIZE)*2
	src	= (RETSIZE+2)*2
	nsrc	= (RETSIZE+1)*2

	; ローカル変数
	yb = -2			; STEP 1 のみ

	d = -2			; dx,dy
	oc = -4
	y = -6
	x = -8
	radr = -10
	last_oc = -12
	i = -14

	;---------------------------
	; 最初のステップ:
	;		完全判定（完全に中か、完全に外か）

	; src
	; nsrc
	; (i,x,y)
	; (xl,xr,yt,yb)

	mov	BX,[BP+nsrc]	;
	dec	BX
	shl	BX,2		; 
	_les	SI,[BP+src]

	; xl,xr,yt,ybの初期設定
	LODE	AX,[SI+BX]
	mov	DX,AX		; DX = xl
	mov	CX,AX		; CX = xr
	LODE	AX,[SI+BX+2]
	mov	DI,AX		; DI = yt
	mov	[BP+yb],AX
	sub	BX,4
	; ループ～
LOOP1:
	LODE	AX,[SI+BX]
	cmp	AX,DX
	jg	short L1_010
	mov	DX,AX
L1_010:	cmp	AX,CX
	jl	short L1_020
	mov	CX,AX
L1_020:
	LODE	AX,[SI+BX+2]
	cmp	AX,DI
	jg	short L1_030
	mov	DI,AX
L1_030:	cmp	AX,[BP+yb]
	jle	short L1_040
	mov	[BP+yb],AX
L1_040:	sub	BX,4
	jge	short LOOP1

	; 全点走査が終ったので、BBOXによる内外判定
LOOP1E:
	; 外判定
	cmp	ClipXL,CX	; CX=xr
	jg	short RET_OUT
	cmp	DX,ClipXR	; DX=xl
	jg	short RET_OUT
	cmp	ClipYB,DI	; DI=yt
	jl	short RET_OUT
	mov	AX,[BP+yb]
	cmp	ClipYT,AX
	jg	short RET_OUT

	; 内判定
	cmp	DX,ClipXL	; DX=xl
	jl	short STEP2
	cmp	CX,ClipXR	; CX=xr
	jg	short STEP2
	cmp	DI,ClipYT
	jl	short STEP2
	mov	AX,ClipYB
	cmp	[BP+yb],AX
	jg	short STEP2
	mov	AX,[BP+dest]
	cmp	SI,AX		; SI = src
	jne	short RET_IN
if datasize EQ 2
	mov	AX,ES		; segment compare
	mov	SI,[BP+dest+2]
	cmp	SI,AX
	jne	short RET_IN
endif
	mov	AX,[BP+nsrc]
	MRETURN

RET_IN:	; 完全に中だぜ
	mov	AX,-1
	MRETURN

RET_OUT: ; 完全に外だぜ
	xor	AX,AX
	MRETURN

	;---------------------------
	; 第２ステップ:
	;		上下のカット
STEP2:
	mov	SI,[BP+ndest]	; SI=nans

	_les	BX,[BP+src]
	LODE	AX,[BX]
	mov	[BP+x],AX	; x = src[0].x
	LODE	AX,[BX+2]
	mov	[BP+y],AX	; y = src[0].y
	GETOUTCODE_Y	BX,AX,ClipYT,ClipYB
	mov	[BP+last_oc],BX	; last_oc = outcode_y(x,y)
	mov	DI,[BP+nsrc]
	dec	DI
	jns	short STEP2_1
	jmp	STEP3		; ここを通るときはSI=nansでなきゃいかんよ
				; 変更するときは気を付けよう

STEP2_1:
	mov	AX,DI
	shl	AX,2
	add	AX,[BP+src]
	mov	[BP+radr],AX	; radr = &src[nsrc-1]
	mov	[BP+i],DI	; i = nsrc-1
	mov	DI,[BP+y]	; DI=y
LOOP2:
	mov	DX,[BP+x]	; DX=x
	mov	CX,DI		; CX=y
	mov	BX,[BP+radr]
	ESSRC
	LODE	AX,[BX]		; AX = x = radr->x
	mov	[BP+x],AX	;
	LODE	DI,[BX+2]	; DI = radr->y
	cmp	DX,AX
	jne	short S2_010
	cmp	CX,DI
	jne	short S2_010
	jmp	S2_090		; (;_;)
S2_010:
	sub	DX,AX
	sub	CX,DI
	GETOUTCODE_Y	AX,DI,ClipYT,ClipYB
	mov	[BP+oc],AX
	mov	BX,[BP+last_oc]
	cmp	AX,BX
	je	short S2_070

	mov	[BP+d],DX	; dx

	; 線分の始点側の切断
	or	BX,BX
	je	short S2_040

	cmp	BX,TOP_OUT
	jne	short S2_020
	mov	AX,ClipYT
	jmp	short S2_030
S2_020:	mov	AX,ClipYB
S2_030:	cmp	AX,DI		;y
	je	short S2_040

	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	STOE	[BX+2],AX	; wy
	sub	AX,DI		; y
	imul	DX		; dx
	idiv	CX		; dy
	add	AX,[BP+x]
	STOE	[BX],AX

S2_040:
	; 線分の終点側の切断
	mov	BX,[BP+oc]
	or	BX,BX
	je	short S2_070
	cmp	BX,TOP_OUT
	jne	short S2_050
	mov	AX,ClipYT
	jmp	short S2_060
	EVEN
S2_050:	mov	AX,ClipYB
S2_060:
	mov	DX,CX		; dy
	add	DX,DI		;y
	cmp	DX,AX
	je	short S2_070

	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	STOE	[BX+2],AX

	sub	AX,DI		;y
	imul	WORD PTR [BP+d]	; dx
	idiv	CX		; dy
	add	AX,[BP+x]
	STOE	[BX],AX
S2_070:
	mov	CX,[BP+oc]
	or	CX,CX
	jne	short S2_080
	dec	SI
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	mov	AX,[BP+x]
	STOE	[BX],AX
	STOE	[BX+2],DI	;y
S2_080:
	mov	[BP+last_oc],CX
S2_090:
	sub	WORD PTR [BP+radr],4
	dec	WORD PTR [BP+i]
	js	short STEP3
	jmp	LOOP2		; (;_;)

	;---------------------------
	; 第３ステップ:
	;		左右のカット
	; SI: nans
STEP3:
	mov	CX,[BP+ndest]
	mov	BX,CX
	shl	BX,2
	add	BX,[BP+dest]
	ESDEST
	LODE	AX,[BX-2]
	mov	[BP+y],AX
	LODE	DI,[BX-4]
	GETOUTCODE_X	AX,DI,ClipXL,ClipXR
	mov	[BP+last_oc],AX
	cmp	SI,CX
	jl	short S3_010
	xor	AX,AX	; nans=0だから
	jmp	RETURN
	EVEN
S3_010:
	mov	AX,SI
	shl	AX,2
	add	AX,[BP+dest]
	mov	[BP+radr],AX
	mov	AX,CX
	sub	AX,SI
	mov	[BP+i],AX
	mov	SI,0		; nans=0
LOOP3:
	mov	CX,DI		; CX:dx
	mov	AX,[BP+y]
	mov	DX,AX		; DX:dy
	mov	BX,[BP+radr]
	LODE	DI,[BX]		; DI:x
	LODE	AX,[BX+2]
	mov	[BP+y],AX
	sub	CX,DI
	sub	DX,AX

	mov	BX,DI
	GETOUTCODE_X	AX,BX,ClipXL,ClipXR
	mov	[BP+oc],AX
	mov	BX,[BP+last_oc]
	cmp	AX,BX
	je	short S3_070

	mov	[BP+d],DX	; dy

	or	BX,BX
	je	short S3_040
	cmp	BX,LEFT_OUT
	jne	short S3_020
	mov	AX,ClipXL
	jmp	short S3_030
	EVEN
S3_020:
	mov	AX,ClipXR
S3_030:
	cmp	AX,DI
	je	short S3_040
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],AX
	sub	AX,DI		; x
	imul	DX
	idiv	CX
	add	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_040:
	mov	BX,[BP+oc]
	or	BX,BX
	je	short S3_070
	cmp	BX,LEFT_OUT
	jne	short S3_050
	mov	AX,ClipXL
	jmp	SHORT S3_060
S3_050:
	mov	AX,ClipXR
S3_060:
	mov	BX,CX
	add	BX,DI
	cmp	BX,AX
	je	short S3_070
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],AX
	sub	AX,DI		; x
	imul	WORD PTR [BP+d]	; dy
	idiv	CX	; dx
	add	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_070:
	mov	CX,[BP+oc]
	or	CX,CX
	jne	short S3_080
	mov	BX,SI
	shl	BX,2
	add	BX,[BP+dest]
	STOE	[BX],DI
	mov	AX,[BP+y]
	STOE	[BX+2],AX
	inc	SI
S3_080:
	mov	[BP+last_oc],CX
	add	WORD PTR [BP+radr],4
	dec	WORD PTR [BP+i]
	je	short S3_090
	jmp	LOOP3		; (;_;)
S3_090:
	mov	AX,SI
RETURN:
	MRETURN
endfunc

END
