;BFILE構造体
BFILE   	struc
	b_hdl	dw	?	; int		ファイルハンドル
	b_left	dw	?	; unsigned	バッファ中の残りデータ数
	b_pos	dw	?	; unsigned	現在のバッファ上ポインタ
	b_siz	dw	?	; unsigned	バッファサイズ
	b_buff	db	?	; unsigned char[1] 一括読み込み用バッファ
BFILE	    	ends


;	エラー番号
;	pferr	enum	{		; エラー情報
	PFEZERO = 0		; 0 初期値
	PFENOTOPEN = 1		; 1 ファイルをオープンできない
	PFENOTFOUND= 2		; 2 ファイルがパックファイル中にない
	PFENOMEM = 3		; 3 メモリを確保できない
	PFERESERVE0 = 4		; 4 予約
	PFEUNKNOWN = 5		; 5 圧縮タイプが不正
	PFEILPFILE = 6 		; 6 パックファイルがpar形式でない
	PFEILEXE = 7		; 7 EXEファイルが不正
	PFEINTERNAL = 0ffh	; 255 内部エラー
;	}

; バッファサイズデフォルト値
	BBUFSIZE equ	512

	CR	equ	0dh
	LF	equ	0ah
	EOF	equ	-1

;	各種マクロ

	clr	macro	reg
		xor	reg, reg
		endm

	chk	macro	reg
		or	reg, reg
		endm

;----------------------------------------------------------------------------
;	for MS-DOS function call
;----------------------------------------------------------------------------

msdos	macro	req, param	; call msdos function
	ifnb	<param>
		mov	ax, req * 256 + param
	else
		mov	ah, req
	endif
		int	21h
	endm

DisplayChar		equ	02h	; 文字のスクリーン出力
DisplayString		equ	09h	; ストリングのスクリーン出力
SetInterruptVector	equ	25h	; 割り込みベクタのセット
GetDate			equ	2ah	; 日付を得る
KeepProcess		equ	31h	; キーププロセス
GetInterruptVector	equ	35h	; 割り込みベクタを得る
GetSetSwitch		equ	37h	; スイッチキャラクタを得る/セットする
CreateHandle		equ	3ch	; ハンドルの作成
OpenHandle		equ	3dh	; ハンドルのオープン
CloseHandle		equ	3eh	; ハンドルのクローズ
ReadHandle		equ	3fh	; リードハンドル
WriteHandle		equ	40h	; ライトハンドル
MoveFilePointer		equ	42h	; ファイルポインタの移動
IOCTL			equ	44h	; IOCTL
DuplicateFileHandle	equ	45h	; ファイルハンドルの二重化
ForceDuplicateFileHandle equ	46h	; ファイルハンドルの強制二重化
AllocateMemory		equ	48h	; メモリの割り当て
FreeMemory		equ	49h	; 割り当てられたメモリの解放
SetBlock		equ	4ah	; 割り当てられたメモリブロックの変更
EndProcess		equ	4ch	; プロセスの終了
SetPSP			equ	50h	; PSPアドレスのセット
GetDOSarea		equ	52h	; 内部変数領域のアドレスの取得
CopyPSP			equ	55h	; PSPの複写
GetSetDateTimeofFile	equ	57h	; ファイルの日付/時刻を得る/セットする
LockUnlock		equ	5ch	; ファイルアクセスのロック/アンロック
GetPSP			equ	62h	; PSPを取得


;	PFILE構造体

PFILE   	struc
	pf_bf	dw	?	; BFILE構造体のセグメント
	pf_getc	dw	?	; int (near *)(pf_t) pfgetc()用関数
	pf_getx	dw	?	; int (near *)(pf_t) getc関数
	pf_size	dd	?	; unsigned long		パックサイズ
	pf_read	dd	?	; unsigned long		読込んだバイト数(物理)
	pf_home	dd	?	; unsigned long		ファイル先頭
	pf_loc	dd	?	; unsigned long		ロケーション(論理)
	pf_osize dd	?	; unsigned long		オリジナルサイズ
;	pf_time	dd	?	; unsigned long		タイムスタンプ ※
	pf_cnt	dw	?	; int			展開用ワーク
	pf_ch	dw	?	; int			〃
	pf_key	db	?	; unsigned char		復号化用ワーク
				;			※→未使用
PFILE	    	ends
