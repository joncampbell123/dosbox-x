PAGE 98,120
; graphics - tile
;
; DESCRIPTION:
;	タイルパターンの設定
;
; FUNCTION:
;	void far _pascal grc_settile( char * tile, int tilelen ) ;
;	void far _pascal grc_settile_solid( int color ) ;

; PARAMETERS:
;	char * tile	
;	int len		
;
;	int color	
;
; BINDING TARGET:
;	Microsoft-C / Turbo-C
;
; RUNNING TARGET:
;	NEC PC-9801 Normal mode
;
; REQUIRING RESOURCES:
;	CPU: V30
 ;
; COMPILER/ASSEMBLER:
;	OPTASM 1.6
;
; NOTES:
;	　grcg_settile()で設定するのは、「タイルパターン自体」では
;	なく、「タイルパターンの格納されたアドレス」であることに注意
;	してください。ということは、grcg_settile()呼び出し後、設定した
;	アドレスからのタイルデータを変更すると、その後の描画パターン
;	が変更されることになります。また、タイルパターンを格納していた
;	メモリを開放するときは、先にgrcg_settile_solid()などを使って
;	タイルパターンのアドレスを変更してください。
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	
;
; HISTORY:
;	92/6/13	Initial

	.186
	.MODEL SMALL
	.DATA?

_TileLen	dw	?	; タイルのライン数
_TileStart	dw	?	; タイルの先頭アドレスoffset
_TileSeg	dw	?	; タイルの先頭アドレスsegment
_TileLast	dw	?	; タイルの最後のラインの先頭offset

	EXTRN _TileSolid:WORD

	.CODE
	include func.inc

; int far pascal grc_settile( char * tile, int tilelen ) ;
func GRC_SETTILE
	enter 0,0
	mov	BX,[BP+(RETSIZE+2)*2]	; tile
	mov	CX,[BP+(RETSIZE+1)*2]	; tilelin
	xor	AX,AX
	or	CX,CX
	jle	short RETURN	; ライン数がゼロ以下だと駄目

	mov	_TileStart,BX
	mov	_TileSeg,DS	; ラージデータモデル未対応
	mov	_TileLen,CX
	dec	CX
	shl	CX,2
	add	BX,CX
	mov	_TileLast,BX
	mov	AX,1
RETURN:
	leave
	ret 4+(DATASIZE+1)*2
endfunc


; void far pascal grc_settile_solid( int color ) {
func GRC_SETTILE_SOLID
	enter 0,0
	mov	BX,[BP+(RETSIZE+1)*2]	; color

	and	BX,0Fh
	shl	BX,2
	add	BX,offset _TileSolid
	mov	_TileStart,BX
	mov	_TileLast,BX
	mov	_TileSeg,seg _TileSolid
	mov	_TileLen,1
	leave
	ret 2
endfunc
END
