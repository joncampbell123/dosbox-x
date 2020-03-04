; master library - FEP
;
; Description:
;	日本語FEPの制御
;
; Function/Procedures:
;	int fep_exist(void) ;
;	int fep_shown(void) ;
;	void fep_show(void) ;
;	void fep_hide(void) ;
;
; Parameters:
;	none
;
; Returns:
;	fep_exist:
;		FEPTYPE_UNKNOWN	存在しない
;		FEPTYPE_IAS	$IAS
;		FEPTYPE_MSKANJI	MS-KANJI API
;	fep_shown:
;		0 消えている
;		1 ついてる
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	日本語MS-DOS, DR-DOS/V, AX-DOS, MS-DOS/V, IBM DOS/V, IBM 日本語DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	事前に必ず get_machineを実行しないと $IASは検査しません
;	その後、fep_existを実行しないと他のfep_*は動作しません
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
;	94/ 1/12 Initial: fepexist.asm/master.lib 0.22
;	94/ 3/ 3 [M0.23] BUGFIX fep_exist 戻り値にfep_typeの値を返さなかった

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	Machine_State:WORD
PCAT	EQU	00010000b

fep_type	dw 0		; FEPの種類

IasDevName	db '$IBMAIAS',0	; $IAS.SYSのデバイス名
MsKanjiDevName	db 'MS$KANJI',0	; MS-KANJIのデバイス名
MsKanjiAPIEntry	dd 0		; MS-KANJIのエントリアドレス

FEPTYPE_UNKNOWN equ 0
FEPTYPE_IAS	equ 1
FEPTYPE_MSKANJI	equ 2

; MS Kanji API function code (wFunc)
KKAsk	equ 1	; カナ漢字変換システム名を調べる
KKOpen	equ 2	; カナ漢字変換システムの使用を開始する
KKClose	equ 3	; カナ漢字変換システムの使用を終了する
KKInOut	equ 4	; カナ漢字変換システムとのデータ入出力を行う
KKMode	equ 5	; カナ漢字入力モードの ON/OFF を参照／設定する

Funcparm	STRUC
wFunc		dw	?	; ファンクション番号
wMode		dw	?	; 漢字入力モードの参照／設定のフラグ
lpKkname	dd	0	; カナ漢字変換システム名用構造体へのポインタ
lpDataparm	dd	0	; データ･パラメータ用構造体へのポインタ
Reserved	dd	0	; 拡張用 (全要素 0 としておく)
Funcparm	ENDS

Dataparm	STRUC
wType		dw	?	; 表示切換スイッチ
wScan		dw	?	; キー･スキャン･コード
wAscii		dw	?	; 文字コード
wShift		dw	?	; シフト･キー･ステータス
wExShift	dw	?	; 拡張シフト･キー･ステータス

cchResult	dw	?	; 確定文字列のバイト数
lpchResult	dd	?	; 確定文字列の格納ポインタ

cchMode		dw	?	; モード表示文字列のバイト数
lpchMode	dd	?	; モード表示文字列の格納ポインタ
lpattrMode	dd	?	; モード表示文字列の属性の格納ポインタ

cchSystem	dw	?	; システム文字列のバイト数
lpchSystem	dd	?	; システム文字列の格納ポインタ
lpattrSystem	dd	?	; システム文字列の属性の格納ポインタ

cchBuf		dw	?	; 未確定文字列のバイト数
lpchBuf		dd	?	; 未確定文字列の格納ポインタ
lpattrBuf	dd	?	; 未確定文字列の属性の格納ポインタ
cchBufCursor	dw	?	; 未確定文字列中のカーソル位置

DReserved	db 34 dup (?)	; 拡張用 (全要素 0 としておく)
Dataparm	ENDS


; DBCS keyboardの状況モード(IAS)
IAS_KANJI    equ 10000000b
IAS_ROMA     equ 01000000b
IAS_HIRAGANA equ 00000100b
IAS_KATAKANA equ 00000010b
IAS_EISUU    equ 00000000b
IAS_ZENKAKU  equ 00000001b

	.CODE

func FEP_EXIST	; fep_exist() {
	mov	fep_type,FEPTYPE_UNKNOWN
	test	Machine_State,PCAT
	jz	short TEST_MSKANJI

	; $IASが存在するかな?
	mov	DX,offset IasDevName
	mov	AX,3d00h		; open
	int	21h
	jc	short TEST_MSKANJI
	mov	BX,AX
	mov	AH,3eh			; close
	int	21h
	mov	fep_type,FEPTYPE_IAS
	jmp	short EXIST_DONE
	EVEN

	; MS KANJI APIが存在するかな?
TEST_MSKANJI:
	mov	DX,offset MsKanjiDevName
	mov	AX,3d00h
	int	21h
	jc	short EXIST_DONE
	mov	BX,AX

	mov	DX,offset MsKanjiAPIEntry
	mov	CX,4
	mov	AX,4402h		; IOCTL read
	int	21h
	push	AX
	mov	AH,3eh			; close
	int	21h
	pop	AX
	cmp	AX,4
	jne	short EXIST_DONE
	mov	fep_type,FEPTYPE_MSKANJI
EXIST_DONE:
	mov	AX,fep_type
	ret
endfunc		; }


; In:
;	CX = wFunc
;	AX = wMode
; Out:
;	AX = wMode
KKfunc	proc	near
	push	BP
	mov	BP,SP
	sub	SP,type Funcparm
	Funcbuf EQU [BP-(type Funcparm)]

	cmp	word ptr MsKanjiApiEntry+2,0
	je	short KKfunc_done

	mov	Funcbuf.wFunc,CX
	mov	Funcbuf.wMode,AX
	xor	BX,BX
	mov	word ptr Funcbuf.lpKkname,BX
	mov	word ptr Funcbuf.lpKkname+2,BX
	mov	word ptr Funcbuf.lpDataparm,BX
	mov	word ptr Funcbuf.lpDataparm+2,BX
	mov	word ptr Funcbuf.Reserved,BX
	mov	word ptr Funcbuf.Reserved+2,BX
	push	SS
	lea	AX,Funcbuf
	push	AX
	call	MsKanjiAPIEntry		; MS-KANJIの呼び出し
	mov	AX,Funcbuf.wMode
KKfunc_done:
	mov	SP,BP
	pop	BP
	ret
KKfunc	endp

func FEP_SHOWN	; fep_shown() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short SHOWN_IAS
	jc	short HIDDEN		; 0
SHOWN_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; 参照
	call	KKfunc
	test	AX,2			; ON bit
	jnz	short SHOWN
HIDDEN:
	xor	AX,AX
	ret

SHOWN_IAS:
	mov	AX,1402h	; シフト状況の取得
	int	16h
	cmp	AL,0		; シフト状況は表示?
	jne	short HIDDEN
	mov	AX,1301h	; DBCS keyboard modeの取得
	int	16h
	test	DL,IAS_KANJI
	jz	short HIDDEN
SHOWN:
	mov	AX,1
	ret
endfunc		; }

func FEP_SHOW	; fep_show() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short ON_IAS
	jc	short ON_DONE		; 0
ON_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; 参照
	call	KKfunc
	test	AX,2			; ON bit
	jnz	short ONMS_DONE
	mov	CX,KKMode
	xor	AX,8003h		; ON/OFF交換, 取得/設定交換
	call	KKfunc
ONMS_DONE:
	ret
	EVEN

ON_IAS:
	mov	AX,1402h	; シフト状況の取得
	int	16h
	cmp	AL,1		; シフト状況は消去?
	jne	short ONIAS_1
	mov	AX,1400h	; 消えているならシフト表示
	int	16h
ONIAS_1:
	cmp	AL,0		; シフト状況は表示?
	jne	short ON_DONE
	mov	AX,1301h	; DBCS keyboard modeの取得
	int	16h
	test	DL,IAS_KANJI
	jnz	short ON_DONE
	or	DL,IAS_KANJI
	test	DL,IAS_ROMA
	jnz	short ONIAS_2
	or	DL,IAS_ZENKAKU+IAS_HIRAGANA
ONIAS_2:
	mov	AX,1300h	; DBCS keyboard modeの設定
	int	16h
ON_DONE:
	ret
endfunc		; }

func FEP_HIDE	; fep_hide() {
	cmp	fep_type,FEPTYPE_IAS	; 1
	je	short OFF_IAS
	jc	short OFF_DONE		; 0
OFF_MSKANJI:
	mov	CX,KKMode
	mov	AX,0			; 参照
	call	KKfunc
	test	AX,1			; OFF bit
	jnz	short OFFMS_DONE
	mov	CX,KKMode
	xor	AX,8003h		; ON/OFF交換, 取得/設定交換
	call	KKfunc
OFFMS_DONE:
	ret

OFF_IAS:
	mov	AX,1402h	; シフト状況の取得
	int	16h
	cmp	AL,0
	jne	short OFF_DONE
	mov	AX,1301h	; DBCS keyboard modeの取得
	int	16h
	cmp	DL,IAS_KANJI
	jz	short OFF_DONE
	xor	DL,IAS_KANJI
	mov	AX,1300h	; DBCS keyboard modeの設定
	int	16h
OFF_DONE:
	ret
endfunc		; }

END
