; master library - MS-DOS
;
; Description:
;	path を4つの構成要素に分解する( \ -> /変換つき )
;
; Functions/Procedures:
;	void file_splitpath_slash(char *path,char *drv,char *dir,char *name,char *ext);
;
; Parameters:
;	path	分解したい元パス名( e.g. "A:\HOGE\ABC.C" )	NULL禁止
;	drv	ドライブ部分の格納先( "A:" )			NULL可
;	dir	パス部分の格納先( "/HOGE/" )			NULL可
;	base	ファイル名部分の格納先("ABC")			NULL可
;	ext	拡張子部分の格納先(".C")			NULL可
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;
;
; Requiring Resources:
;	CPU: 8086
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
;	紳ちゃん
;	恋塚
;
; Revision History:
;	94/ 5/29 Initial: filsplsl.asm/master.lib 0.23
;	95/ 3/10 [M0.22k] NULL可の引き数が全くNULL不可だったのを訂正

	.MODEL SMALL
	include	func.inc

EOS	equ	0	;C文字列の終端文字

if DATASIZE eq 2
	_seg	equ	ES
else
	_seg	equ	DS
endif

; COPYSTR arg
; in:
;  arg   (ES:)DIにいれるデータ
;  SI    読み取り元
;  CX    転送したい長さ
; out:
;  (ES:)DIがNULLならば、SI,CXはそのまま。
;  NULLでなければ、ES:DIに文字列が複写され、SI,DIにCXが加算され、CX=0になる。
;
;  AX破壊
COPYSTR macro arg
	local SKIP
	_les	DI,arg
	_mov	AX,ES
    l_ <or	AX,DI>
    s_ <test	DI,DI>
	jz	short SKIP
	rep	movsb
	mov	byte ptr _seg:[DI],EOS
SKIP:
endm

	.CODE

func	FILE_SPLITPATH_SLASH	; file_splitpath_slash() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI
	_push	DS

	path	= (RETSIZE + 1 + DATASIZE*4)*2
	drv	= (RETSIZE + 1 + DATASIZE*3)*2
	dir	= (RETSIZE + 1 + DATASIZE*2)*2
	_name	= (RETSIZE + 1 + DATASIZE*1)*2
	ext	= (RETSIZE + 1 + DATASIZE*0)*2

	s_mov	AX,DS
	s_mov	ES,AX

;ディレクトリ区切りを '/' に変換する
;1バイト目が 0x81-0x9f,0xe0-0xef ならば、シフトJIS漢字であるとしている
	_lds	SI,[BP+path]
transloop:
	lodsb
	or	AL,EOS
	jz	short transend
	cmp	AL,'\'
	jne	short nottrans
	mov	byte ptr [SI - 1],'/'
nottrans:
	cmp	AL,081h
	jb	short transloop
	cmp	AL,09fh
	jbe	short skipbyte
	cmp	AL,0e0h
	jb	short transloop
	cmp	AL,0efh
	jnbe	short transloop
skipbyte:
	lodsb
	or	AL,EOS
	jnz	short transloop
transend:
	mov	BX,SI	;BX = 文字列末 + 1

;ドライブ名を得る、2文字以下ならドライブ名は存在しない
	mov	CX,0
	sub	SI,[BP+path]		; SI=長さ
	cmp	SI,2
	mov	SI,[BP+path]		; SI=元パス先頭
	jb	short drvend
	cmp	byte ptr [SI + 1],':'
	jne	short drvend
	mov	CX,2
drvend:
	COPYSTR	[BP+drv]
	add	SI,CX

;ディレクトリ名を得る
	_mov	AX,DS		;scas準備
	_mov	ES,AX
	lea	DI,[BX - 1]	;DI = 文字列末
	mov	CX,BX
	sub	CX,SI		;CX = 文字列長

	; '/'を文字列末尾から検索する
 	xor	AX,AX		;
	or	AL,'/'		;with ZF = 0
	STD			;デクリメント検索
	repne	scasb
	CLD			;すぐ戻す
	jne	short NO_DIR	; 見つからなければ CX = 0 になってる
	inc	CX		; 見つかったらCX = len+1('/'を含ませるため)
NO_DIR:
	COPYSTR	[BP+dir]
	add	SI,CX

;ファイル名を得る
;CX = 文字列長、検索 -> CX = 拡張子長、∴ DX-CX = ファイル名長
	_mov	AX,DS		;scas準備
	_mov	ES,AX
	mov	CX,BX
	sub	CX,SI		;
	mov	DX,CX		;DX = CX = 文字列長
	mov	DI,SI
	xor	AX,AX		;
	or	AL,'.'		;with ZF = 0
	repne	scasb
	jne	short noext
	inc	CX
noext:
	sub	DX,CX
	xchg	DX,CX
	COPYSTR	[BP+_name]
	add	SI,CX

	mov	CX,DX
	cmp	CX,4
	jle	short do_ext
	mov	CX,4		; 拡張子は4文字リミットをかける
do_ext:
	COPYSTR	[BP+ext]

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	DATASIZE*10
endfunc			; }

END
