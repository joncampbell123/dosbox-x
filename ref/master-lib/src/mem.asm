; master library
;
; Description:
;	メモリマネージャの作業変数
;
; Variables:
;	mem_TopSeg	メモリマネージャが管理する先頭のセグメント
;	mem_OutSeg	メモリマネージャが管理する末尾の次のセグメント
;	mem_EndMark	スタック型メモリマネージャの次に確保する位置
;	mem_TopHeap	ヒープ型メモリマネージャの確保した先頭
;	mem_FirstHole	ヒープ型メモリマネージャの開けた最初の穴
;	mem_AllocID	メモリ確保時にブロック管理部書き込まれる用途ID
;       mem_Reserve     mem_assign_allで確保するときに残す大きさ
;
; Revision History:
;	93/ 6/12 [M0.19] バージョン文字列強制リンク
;	93/ 9/15 [M0.21] mem_TopSeg を強制ゼロ初期化
;	95/ 2/14 [M0.22k] mem_AllocID追加
;       95/ 3/ 3 [M0.22k] mem_Reserve追加

	.MODEL SMALL

	; バージョン文字列をリンクする
	EXTRN _Master_Version:BYTE

	.DATA
	public mem_TopSeg,  mem_OutSeg
	public _mem_TopSeg, _mem_OutSeg
	public mem_TopHeap,  mem_FirstHole,  mem_EndMark
	public _mem_TopHeap, _mem_FirstHole, _mem_EndMark
	public mem_MyOwn, _mem_MyOwn
	public mem_AllocID, _mem_AllocID
	public mem_Reserve, _mem_Reserve

_mem_TopSeg label word
mem_TopSeg	dw	0
	public mem_MyOwn
_mem_MyOwn label word
mem_MyOwn	dw	0

	public mem_AllocID
_mem_AllocID label word
mem_AllocID	dw	0

	public mem_Reserve
_mem_Reserve label word
mem_Reserve	dw	256		; 4096 bytes空ける

	.DATA?
_mem_OutSeg label word
mem_OutSeg	dw	?
_mem_TopHeap label word
mem_TopHeap	dw	?
_mem_FirstHole label word
mem_FirstHole	dw	?
_mem_EndMark label word
mem_EndMark	dw	?

END
