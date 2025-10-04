PAGE 98,120
; graphics - clip - line
;
; DESCRIPTION:
;	直線の描画(青プレーンのみ)
;
; FUNCTION:
;	int _pascal grc_clip_line( Point * p1, Point * p2 ) ;
;
; PARAMETERS:
;	Point * p1	始点の座標
;	Point * p2	終点の座標
;
; Returns:
;	0	完全に枠外(クリッピングによって消滅する)
;	1	枠内の直線座標が生成された
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	8086
;
; REQUIRING RESOURCES:
;	CPU: 8086
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; NOTES:
;	枠外の時は *p1, *p2 の値は変化しません。
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	grc_setclip()
;	clipline
;
; HISTORY:
;	93/ 3/ 1 Initial ( from gc_line.asm )
;	93/ 6/ 6 [M0.19] small data modelでの暴走fix

	.MODEL SMALL

	.DATA

	; クリッピング関係
	EXTRN	ClipXL:WORD
	EXTRN	ClipXR:WORD
	EXTRN	ClipYT:WORD
	EXTRN	ClipYH:WORD
	EXTRN	ClipYT_seg:WORD

	.CODE
	include func.inc
	include clip.inc

; カットカット～～～(x1,y1)を切断により変更する
; in: BH:o2, BL:o1
; out: BH:o2, BL:o1, o1がzeroならば zflag=1

;
MRETURN macro
	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	2*DATASIZE*2
	EVEN
	endm

;-------------------------------------------------------------
; int _pascal grc_clip_line( Point *p1, Point *p2 ) ;
;
func GRC_CLIP_LINE
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS
	PUSHED = (DATASIZE+1)*2	; SI,DI,[DS]

	; 引数
	p1	= (RETSIZE+1+DATASIZE)*2
	p2	= (RETSIZE+1)*2

	_lds	BX,[BP+p1]
	mov	CX,[BX]		; x1
	mov	DI,[BX+2]	; y1

	_lds	BX,[BP+p2]
	mov	SI,[BX]		; x2
	mov	BP,[BX+2]	; y2

	; クリッピング開始 =================================

	; クリップ枠に合わせてy座標を変換
	mov	AX,ClipYT
	sub	DI,AX
	sub	BP,AX

	mov	AX,ClipXL
	mov	DX,ClipYH
	mov	BX,0505h	; outcode を全部左下に!!
	GETOUTCODE BL,CX,DI,AX,ClipXR,DX
	GETOUTCODE BH,SI,BP,AX,ClipXR,DX
	test	BH,BL
	jnz	short CLIPOUT	; まず初期検査。クリップアウト～
	or	BX,BX
	jz	short OK

	call	cutline		; 第１点のクリップ
	jz	short OK
	xchg	BH,BL
	xchg	CX,SI
	xchg	DI,BP
	call	cutline		; 第２点のクリップ
	jnz	short CLIPOUT

	xchg	CX,SI		; 前後関係をもとにもどす
	xchg	DI,BP

	; +-----+-----+-----+-----+-----+-----+-----+
	; !AH AL!DH DL!BH BL!CH CL! SI  ! DI  ! BP  !
	; !     !     !     ! x1  ! x2  ! y1  ! y2  !
	; +-----+-----+-----+-----+-----+-----+-----+
OK:
	mov	AX,BP
	mov	BP,SP
	_lds	BX,[BP+p1+PUSHED]
	mov	[BX],CX		; x1
	mov	[BX+2],DI	; y1

	_lds	BX,[BP+p2+PUSHED]
	mov	[BX],SI		; x2
	mov	[BX+2],AX	; y2
	mov	AX,1
	MRETURN

	; クリップアウト
CLIPOUT:
	xor	AX,AX
	MRETURN
endfunc

END
