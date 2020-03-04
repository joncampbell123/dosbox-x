; master library - heap
;
; Description:
;	heapメモリのサイズ変更
;
; Function/Procedures:
;	unsigned hmem_reallocbyte( unsigned oseg, unsigned bytesize ) ;
;	unsigned hmem_realloc( unsigned oseg, unsigned parasize ) ;
;
; Parameters:
;	oseg	  従来のhmemメモリブロック
;	bytesize  新しい大きさ(バイト単位)
;	parasize  新しい大きさ(パラグラフ単位)
;
; Returns:
;	新しいセグメント位置
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	
;
; Assembly Language Note:
;	AX以外のレジスタを保存します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 1 Initial: hmemreal.asm/master.lib 0.22
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/19 [M0.22k] 新サイズが(mem_OutSeg-mem_EndMark)以上なら
;			hmem_allocに渡してエラーにさせるようにした
;	95/ 3/21 [M0.22k] BUGFIX 内容転写がうまく動いていなかった

	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD
	EXTRN mem_EndMark:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN HMEM_ALLOC:CALLMODEL
	EXTRN HMEM_FREE:CALLMODEL

MEMHEAD STRUC
using	dw	?
nextseg	dw	?
mem_id	dw	?
MEMHEAD	ENDS

	oseg = (RETSIZE+2)*2

func HMEM_REALLOCBYTE	; hmem_reallocbyte() {
	push	BX
	mov	BX,SP
	push	CX
	;
	bytesize = (RETSIZE+1)*2
	mov	CX,SS:[BX+oseg]
	mov	BX,SS:[BX+bytesize]
	add	BX,15
	rcr	BX,1
	shr	BX,1
	shr	BX,1
	shr	BX,1
	jmp	short hmem_reallocb
endfunc			; }

NEW_ALLOC proc CALLMODEL	; oseg=0のときは、新規確保
	push	BX
	call	HMEM_ALLOC
	pop	CX
	pop	BX
	ret	4
NEW_ALLOC endp

OLD_FREE proc CALLMODEL		; bytesize|parasize = 0のときは、開放
	push	BX
	call	HMEM_FREE
	xor	AX,AX
	mov	mem_AllocID,AX
	pop	CX
	pop	BX
	ret	4
OLD_FREE endp

func HMEM_REALLOC	; hmem_realloc() {
	push	BX
	mov	BX,SP
	push	CX
	;
	parasize = (RETSIZE+1)*2

	mov	CX,SS:[BX+oseg]
	mov	BX,SS:[BX+parasize]
hmem_reallocb:
	mov	AX,mem_OutSeg
	sub	AX,mem_EndMark
	cmp	BX,AX
	jae	short NEW_ALLOC	; でかすぎは hmem_alloc にエラーにさせる
	jcxz	short NEW_ALLOC
	test	BX,BX
	jz	short OLD_FREE

	dec	CX
	push	ES
	push	SI
	xor	SI,SI		; 0

	inc	BX

	; house keeping
	cmp	CX,mem_TopHeap	; 最低ヒープよりも低いなら zero return
	jb	short NO_GOOD
	cmp	CX,mem_OutSeg	; 最高ヒープアドレスより高いなら zero return
	jae	short NO_GOOD
	mov	ES,CX
	cmp	ES:[SI].using,1	; usingフラグが「使用中(1)」でなければ zero ret
	jne	short NO_GOOD

	; さて本番
	mov	AX,ES:[SI].mem_id	; IDを転写しておく
	mov	mem_AllocID,AX

	mov	AX,ES:[SI].nextseg ; AX=nextseg
	add	BX,CX		; BX= 要求した大きさにしたときの次になる位置
	jc	short OTHER_PLACE	; carryでるなら絶対拡大で他の場所
	cmp	AX,BX
	jae	short SHRINK

	; 大きくなるとき
	cmp	AX,mem_OutSeg
	jae	short OTHER_PLACE	; 最終ブロックなら移動するしかない
	mov	ES,AX
	cmp	ES:[SI].using,SI
	jne	short OTHER_PLACE	; 次のブロックが使用中ならやはり移動

	; 後ろに空間がある
	cmp	ES:[SI].nextseg,BX
	jb	short OTHER_PLACE	; 後ろの空間を足しても足りない?
	; 足りる場合
	cmp	AX,mem_FirstHole
	jne	short CONNECT
	; FirstHoleをつぶすために、確保するのだ
	sub	AX,ES:[SI].nextseg
	not	AX			; (nextseg - freeseg) - 1
	push	AX
	call	HMEM_ALLOC
CONNECT:
	mov	AX,ES:[SI].nextseg
	mov	ES,CX
	mov	ES:[SI].nextseg,AX	; 結合する
	cmp	AX,BX
SHRINK:
	je	short HOP_OK		; 一致したらOK
IF 0
	; in: ES=CX=現在のseg
	inc	BX
	cmp	AX,BX
	je	short HOP_OK		; 目的の末尾+1=現在末尾なら、やはりOK
	dec	BX
ENDIF

	; 縮める
	mov	ES:[SI].nextseg,BX	; BX(理想の次位置)を実現する
	mov	ES,BX
	mov	ES:[SI].nextseg,AX	; その地点の次位置
	mov	ES:[SI].using,1		; 使用中のブロック二つに分断
	inc	BX			; (すぐ開放するからmem_AllocIDは不要)
	push	BX
	call	HMEM_FREE
HOP_OK:	jmp	short OK

NO_GOOD:			; 失敗のときに 0 を返すのはここ
	xor	AX,AX
	jmp	short RETURN

	; 後ろに隙間がないから、別の場所に再確保する
OTHER_PLACE:
	; CX=現在のメモリブロックの位置
	; BX=CXに要求された新しい大きさを加えた次の位置
	; AX=現在の次の位置( ここに来るなら常に BX > AX )
	cmp	CX,mem_TopHeap
	jne	short RE_ALLOC
	; 先頭ならば、足りない大きさだけ前に戻れば良い
	sub	BX,AX	; BX=足りない量
	sub	CX,BX	; CX=その分だけ戻した位置
	jb	short RE_ALLOC1		; 95/3/22
	cmp	CX,mem_EndMark
	jb	short RE_ALLOC1		; やっぱり足りない場合
	mov	ES,CX
	mov	ES:[SI].nextseg,AX
	mov	ES:[SI].using,1
	push	mem_AllocID
	pop	ES:[SI].mem_id
	mov	mem_TopHeap,CX
	add	BX,CX
	sub	AX,BX
	; CX=新しい位置(管理領域の位置)
	; BX=古い位置(〃)
	; AX=古い大きさ(拡大のときの処理だから、常に新しい大きさより小さい)
	jmp	short TRANS
	EVEN

	; 真後ろに拡大しようとしてできなかったときの処理
	; ・先頭のブロックなら前に拡大する。
	; ・それができないときは、別の位置に新規確保する。
	; ・どちらにしろ、新しい位置に以前の内容を転写する。

	
	; AX=現在の次の位置
	; BX=足りない量(正数)
	; CX=その分だけ戻した位置
RE_ALLOC1:
	add	CX,BX
	add	BX,AX

	; CX=現在のメモリブロックの位置
	; BX=CXに要求された新しい大きさを加えた次の位置
	; AX=現在の次の位置( ここに来るなら常に BX > AX )

RE_ALLOC:
	sub	AX,CX		; AX=古い大きさ
	sub	BX,CX		; BX=新しい大きさ
	xchg	AX,BX		; AX=新しい大きさ, BX=古い大きさ
	xchg	BX,CX		; BX=古い位置, CX=古い大きさ
	dec	AX
	push	AX
	call	HMEM_ALLOC
	jc	short NO_GOOD
	dec	AX
	xchg	AX,CX
	push	AX
	lea	AX,[BX+1]
	push	AX
	call	HMEM_FREE	; 先に開放しちゃう
	pop	AX

	; 以前の内容を転写する。
	; CX=新しい位置(管理領域の位置)
	; BX=古い位置(〃)
	; AX=古い大きさ(拡大のときの処理だから、常に新しい大きさより小さい)
TRANS:	push	CX
	push	DI
	push	DS
	CLD
TRANSLOOP:				; 手抜きの転送(^^;
	inc	CX
	inc	BX
	mov	ES,CX
	mov	SI,0
	mov	DS,BX
	mov	DI,SI
	mov	CX,16/2
	rep	movsw
	mov	CX,ES
	dec	AX
	jnz	short TRANSLOOP
	pop	DS
	pop	DI
	pop	CX
OK:
	inc	CX
	mov	AX,CX
RETURN:
	mov	mem_AllocID,0
	pop	SI
	pop	ES
	pop	CX
	pop	BX
	ret	4
endfunc		; }

END
