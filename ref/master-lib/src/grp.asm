; master library
;
; Description:
;	グラフィック画面関連のグローバル変数定義
;
; Global Variables:
;	graph_VramSeg	グラフィック画面の先頭セグメントアドレス
;	graph_VramWords	グラフィック画面の1プレーンあたりのワード数
;	graph_VramLines	グラフィック画面の縦ドット数
;	graph_VramWidth	グラフィック画面の1ラインあたりのバイト数
;	graph_VramZoom	下位8bit: グラフィック画面の縦倍シフト量
;			上位8bit: GDC5MHzなら40h, 2.5MHzなら0(graph_[24]00line)
;			(上記は98の場合のみ)
;	graph_MeshByte	{grcg,vgc}_bytemesh_xのメッシュパターン(偶数line)
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/16 Initial
;	92/11/22 VSYNCカウンタ追加
;	93/ 2/10 vsync_Proc追加
;	93/ 6/12 [M0.19] バージョン文字列強制リンク
;	93/ 8/ 8 vsync関連をvs.asmに移動
;	93/12/ 4 [M0.22] graph_VramWidthを追加
;	94/ 1/22 [M0.22] graph_VramZoomを追加
;	95/ 1/20 [M0.23] graph_MeshByteを追加
;
	.MODEL SMALL

	; バージョン文字列をリンクする
	EXTRN _Master_Version:BYTE

	.DATA

	public _graph_VramSeg,graph_VramSeg
graph_VramSeg label word
_graph_VramSeg	dw 0a800h
	public _graph_VramWords,graph_VramWords
graph_VramWords label word
_graph_VramWords	dw 16000
	public _graph_VramLines,graph_VramLines
graph_VramLines label word
_graph_VramLines	dw 400
	public _graph_VramWidth,graph_VramWidth
graph_VramWidth label word
_graph_VramWidth	dw 80
	public _graph_VramZoom,graph_VramZoom
graph_VramZoom label word
_graph_VramZoom	dw 0

	public _graph_MeshByte,graph_MeshByte
graph_MeshByte label word
_graph_MeshByte	db 55h

END
