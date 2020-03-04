; master library
;
; Description:
;	ヒープ型メモリマネージャ
;
; Functions:
;	unsigned hmem_allocbyte( unsigned bytesize ) ;
;	unsigned hmem_alloc( unsigned parasize ) ;
;	void hmem_free( unsigned memseg ) ;
;
; Returns:
;	unsigned hmem_alloc:	 (cy=0) 確保したセグメント
;				0(cy=1) 管理メモリ不足
;
; Notes:
;	hmem_alloc 実行後は、mem_AllocID は 0になります。
;	hmem_alloc(0)として呼び出すと、メモリ不足を返します。
;
;	ヒープの構造
;		管理情報: 16bytes
;			Using	2bytes	使用中なら1, 未使用なら 0
;			NextSeg	2bytes	次のメモリブロックの先頭
;					末尾なら mem_OutSegと同じ値
;			ID	2bytes	用途ID(確保時のmem_AllocIDの値を転写)
;		続くパラグラフがデータ部
;
; Assembly Language Note:
;	AX以外の全てのレジスタを保存します。
;
; Running Target:
;	MS-DOS
;
; Author:
;	恋塚昭彦
;
; Rebision History:
;	93/ 3/20 Initial
;	93/ 5/ 3 bugfix: hmem_allocbyte(2倍の確保がされていた(^^;)
;	93/11/ 7 [M0.21] a=hmem_alloc(x); b=hmem_alloc(y);
;			hmem_free(a); hmem_free(b); としたときに
;			mem_TopHeapが異常になるバグ修正(hmem_freeのbug)
;			(末尾がholeでその直前で唯一のブロックを開放したとき)
;	93/11/ 7 [M0.21] hmem_freeが、holeが連接したときに接続する部分が
;			おかしかった(;_;)
;	93/12/27 [M0.22] hmem_freeの連接接続が完全ではなかった
;	93/12/29 [M0.22] hmem_freeに渡される値の検査を少しいれた
;			(最低値より小さいときと、using flagが1でないときは
;			 何もせずreturnするようにした)
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/19 [M0.22k] 確保サイズが(mem_OutSeg-mem_EndMark)以上なら
;			先に失敗を返すようにした。
;	95/ 3/21 [M0.22k] BUGFIX hmem_free() 先頭のブロックを開放したときに
;			直後がフリーブロックで、その次のフリーブロックが最終
;			ブロックのときに、その最終フリーブロックを見失って
;			mem_FirstHoleを0にしていた。
;			　このため、次に末尾から2番目のブロックを開放しても
;			最初の空きを作ったと思って連接作業を行わず、
;			末尾の"忘れ去られた"フリーブロックと連接しなかった。
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD	; "Hole": フリーブロックのこと
	EXTRN mem_EndMark:WORD
	EXTRN mem_AllocID:WORD

	.CODE

	EXTRN	MEM_ASSIGN_ALL:CALLMODEL

MEMHEAD STRUC
using	dw	?
nextseg	dw	?
mem_id	dw	?
MEMHEAD	ENDS

func HMEM_ALLOCBYTE	; hmem_allocbyte() {
	push	BX
	mov	BX,SP
	;
	bytesize = (RETSIZE+1)*2
	mov	BX,SS:[BX+bytesize]
	add	BX,15
	rcr	BX,1
	shr	BX,1
	shr	BX,1
	shr	BX,1
	jmp	short hmem_allocb
endfunc			; }

func HMEM_ALLOC		; hmem_alloc() {
	push	BX
	mov	BX,SP
	;
	parasize = (RETSIZE+1)*2

	mov	BX,SS:[BX+parasize]
hmem_allocb:
	cmp	mem_TopSeg,0	; house keeping
	jne	short A_S
	call	MEM_ASSIGN_ALL
A_S:
	push	CX
	push	ES

	test	BX,BX
	jz	short NO_MEMORY	; house keeping
	mov	AX,mem_OutSeg
	sub	AX,mem_EndMark
	cmp	BX,AX
	jae	short NO_MEMORY	; house keeping (add 95/3/19)

	inc	BX

	mov	AX,mem_FirstHole
	test	AX,AX
	jz	short ALLOC_CENTER	; 中心から取るのね

	; フリーブロックから探す
SEARCH_HOLE_S:
	mov	CX,mem_OutSeg
SEARCH_HOLE:
	mov	ES,AX
	mov	AX,ES:[0].nextseg
	cmp	ES:[0].using,0
	jne	short SEARCH_HOLE_E
	mov	CX,ES
	add	CX,BX
	jc	short SEARCH_HOLE_E0	; 95/3/22
	cmp	CX,AX	; now+size <= next
	jbe	short FOUND_HOLE
SEARCH_HOLE_E0:
	mov	CX,mem_OutSeg
SEARCH_HOLE_E:
	cmp	AX,CX
	jne	short SEARCH_HOLE
	; フリーブロックにはめぼしいものはなかった

ALLOC_CENTER:
	; 中心から確保するのね
	mov	AX,mem_TopHeap
	mov	CX,AX
	sub	AX,BX
	jc	short NO_MEMORY		; 95/3/22
	cmp	AX,mem_EndMark
	jb	short NO_MEMORY
	mov	mem_TopHeap,AX
	mov	ES,AX
	mov	ES:[0].nextseg,CX
	mov	ES:[0].using,1
	mov	BX,AX
	jmp	short RETURN

NO_MEMORY:			; こんなとこに
	mov	AX,0
	mov	mem_AllocID,AX
	stc
	pop	ES
	pop	CX
	pop	BX
	ret	2

	; ES=now
	; AX=next
	; CX=now+size
FOUND_HOLE:
	; いいフリーブロックが見つかったので
	; 前の必要部分だけを切り取るのだ
	sub	AX,CX
	cmp	AX,1	; 新しい穴が 0～1パラの大きさしかないなら穴を占有する
	jbe	short JUST_FIT
	add	AX,CX
	mov	ES:[0].using,1
	mov	ES:[0].nextseg,CX
	mov	BX,ES
	mov	ES,CX
	mov	ES:[0].nextseg,AX
	mov	ES:[0].using,0
	cmp	BX,mem_FirstHole
	jne	short RETURN
	mov	mem_FirstHole,CX ; 今のが先頭フリーブロックだったら更新だ
	jmp	short RETURN

	; ちょうどいい按配でフリーブロックが見つかったのね
JUST_FIT:
	mov	ES:[0].using,1
	mov	BX,ES
	cmp	BX,mem_FirstHole
	jne	short RETURN
	; 今つぶしたのが先頭フリーブロックだったのなら検索だ
	mov	AX,mem_OutSeg
	mov	CX,BX
	push	BX
SEARCH_NEXT_HOLE:
	les	CX,ES:[0]	; CX=using, ES=nextseg
	jcxz	short FOUND_NEXT_HOLE
	mov	BX,ES
	cmp	BX,AX
	jb	short SEARCH_NEXT_HOLE
	xor	BX,BX
FOUND_NEXT_HOLE:
	mov	mem_FirstHole,BX
	pop	BX
	; jmp	short RETURN

 ; in: BX = 確保できたメモリの管理ブロックのsegment
RETURN:	mov	ES,BX
	mov	AX,0
	xchg	AX,mem_AllocID
	mov	ES:[0].mem_id,AX
	lea	AX,[BX+1]
	clc
NO_THANKYOU:				; hmem_freeのエラーはここにくる(笑)
	pop	ES
	pop	CX
	pop	BX
	ret	2
endfunc			; }


func HMEM_FREE		; hmem_free() {
	push	BX
	mov	BX,SP
	push	CX
	push	ES
	;
	memseg = (RETSIZE+1)*2
	mov	BX,SS:[BX+memseg]
	dec	BX
	mov	ES,BX
	cmp	BX,mem_TopHeap
	je	short EXPAND_CENTER	; 先頭のブロックなら専用処理へ
	jb	short NO_THANKYOU	; mem_TopHeapより小さいなら無効

	; 先頭ではないブロックの開放処理
	xor	BX,BX
	cmp	ES:[BX].using,1
	jne	short NO_THANKYOU	; usingが1でなければ無効
	mov	ES:[BX].using,BX	; using <- 0
	mov	CX,mem_FirstHole
	mov	AX,ES
	mov	mem_FirstHole,AX
	jcxz	short FREE_RETURN	; 穴がなかったのならすぐOKだ
	cmp	AX,CX
	jb	short CONNECT_START
	mov	AX,CX		; 以前の最初の穴と現在位置のどちらか低い方を
	mov	mem_FirstHole,AX	; 新しい最初の穴に。
CONNECT_START:

	mov	CX,AX
	mov	AX,ES:[BX].nextseg	; 終了地点は、目的ブロックの次だけど

	cmp	AX,mem_OutSeg
	jne	short NO_TAIL
	mov	AX,ES			; 末尾だったら、終了地点をひとつ前へ
NO_TAIL:
	; ・連接してしまったフリーブロックを接続する作業
	push	DS
CONNECT_FREE:
	; 空きを探すループ
	mov	DS,CX			; DS<-CX
	mov	CX,[BX].nextseg		; CX=next seg
	cmp	CX,AX
	ja	short CONNECT_SKIP_OVER	; nextsegが最終segより大きければ終わり
	cmp	[BX].using,BX
	jne	short CONNECT_FREE	; フリーブロックになるまで進める
	; 連続した空きを繋ぐループ
CONNECT_FREE2:
	mov	ES,CX			; ES<-CX(=next seg)
	cmp	ES:[BX].using,BX	; 連接したフリーまで進める
	jne	short CONNECT_FREE
	; DSとESのフリーブロックが連接している
	mov	CX,ES:[BX].nextseg	; CX=新しいnext seg
	mov	[BX].nextseg,CX		; 接続
	cmp	CX,AX
	jbe	short CONNECT_FREE2	; CXが最終seg以内ならまた次を見る
CONNECT_SKIP_OVER:
	pop	DS
	jmp	short FREE_RETURN

	EVEN
EXPAND_CENTER:	; 先頭ブロックの開放
	xor	BX,BX
	mov	AX,ES:[BX].nextseg
	mov	mem_TopHeap,AX
	cmp	AX,mem_OutSeg
	je	short FREE_RETURN	; 末尾だったら終り
	mov	ES,AX
	cmp	ES:[BX].using,BX
	jne	short FREE_RETURN	; 次のブロックが使用中なら終り

	; すぐ次(ES:)がフリーブロックなのでそこも詰める。
	mov	AX,ES:[BX].nextseg
	mov	mem_TopHeap,AX
	; 以後の最初のフリーブロックを検索し、その場所をmem_FirstHoleに入れる
	mov	CX,mem_OutSeg
	cmp	AX,CX
	je	short FREE_LOST_HOLE	; それが最終ブロックなら飛ぶ
	jmp	short X_SEARCH_HOLE
	EVEN
X_SEARCH_NEXT_HOLE:
	mov	AX,ES:[BX].nextseg
	cmp	AX,CX
	je	short FREE_LOST_HOLE
X_SEARCH_HOLE:
	mov	ES,AX
	cmp	ES:[BX].using,BX
	jne	short X_SEARCH_NEXT_HOLE
	mov	BX,ES
FREE_LOST_HOLE:
	mov	mem_FirstHole,BX

FREE_RETURN:
	clc
	pop	ES
	pop	CX
	pop	BX
	ret	2
endfunc			; }

END
