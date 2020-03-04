.xlist
; Description
;	NEAR版とFAR版をアセンブル時に選択設定するためのincludeファイル
;
; Usage:
;	include時に NEARMODEL が定義されていると near版を生成し、
;	定義されていなければ FAR版(Pascal版と共用)を生成する。
;	さらに、COMPACTMODEL, MEDIUMMODELも使える。
;	　アセンブルオプション/Dを使用するとソースの変更なしに両方の
;	生成ができる。
;
; Author:
;	恋塚昭彦
;
; History:
;	92/ 6/19 Initial
;	92/11/16 _pushなどのマクロ追加
;	93/ 2/12 COMPACTMODEL, MEDIUMMODEL

.list
IFDEF NEARMODEL
  CALLMODEL equ NEAR
  RETSIZE EQU 1
  DATASIZE EQU 1
ENDIF
IFDEF COMPACTMODEL
  CALLMODEL equ NEAR
  RETSIZE EQU 1
  DATASIZE EQU 2
ENDIF
IFDEF MEDIUMMODEL
  CALLMODEL equ FAR
  RETSIZE EQU 2
  DATASIZE EQU 1
ENDIF
IFDEF FARMODEL
  CALLMODEL equ FAR
  RETSIZE EQU 2
  DATASIZE EQU 2
ENDIF

.xlist

; FUNC:
; 関数の入り口を指示するマクロ
; <実行するとマクロENDFUNCが生成される。これを用いて関数の終りを指示する>
func	macro	name
	public name
	EVEN
	name proc CALLMODEL
	endfunc macro
		EVEN
		name endp
	endm
endm

; RETFUNC:
; funcの外に、復帰処理をするPROCを置くときに、その先頭を指示するマクロ
; <実行するとマクロENDFUNCが生成される。これを用いて関数の終りを指示する>
retfunc	macro name
	name proc CALLMODEL
	EVEN
	endfunc macro
		EVEN
		name endp
	endm
endm

; リングバッファのポインタを次に進める
; 処理内容:
;	if ( ++reg == bufsize )
;		reg = 0 ;
; 破壊レジスタ:
;	wreg
; 注意:
;	wregとregは同じサイズであること
RING_INC macro reg,bufsize,wreg
	inc	reg
	cmp	reg,bufsize
	sbb	wreg,wreg
	and	reg,wreg
	endm


; destとcmpregを無符号で比較し、小さい方の値をdestに格納する。
; フラグ:
;	結果が 0 なら zflag = 1
;	destのほうがcmpregより小さかったなら cy=1
; 破壊レジスタ:
;	wreg
min2	macro dest,cmpreg,wreg
	sub	dest,cmpreg
	sbb	wreg,wreg
	and	dest,wreg
	add	dest,cmpreg
	endm

; 邪悪なmov (オペランドを自己書き換えラベルとするのね)
; word size専用
JMOV	MACRO	reg,lab
	mov	reg,1234h
	org	$-2
lab	dw	?
	ENDM


IF DATASIZE EQ 2
_push	MACRO reg
	push reg
	ENDM
_pop	MACRO reg
	pop reg
	ENDM
_lds	MACRO reg,mem
	lds reg,mem
	ENDM
_les	MACRO reg,mem
	les reg,mem
	ENDM
_mov	MACRO a,b
	mov a,b
	ENDM
_inc	MACRO a
	inc a
	ENDM
_add	MACRO a,b
	add a,b
	ENDM
s_inc	MACRO a
	ENDM
s_mov	MACRO a,b
	ENDM
s_push	MACRO a
	ENDM
s_pop	MACRO a
	ENDM
s_	MACRO a
	ENDM
l_	MACRO a
	a
	ENDM
ELSE
_push	MACRO reg
	ENDM
_pop	MACRO reg
	ENDM
_lds	MACRO reg,mem
	mov reg,mem
	ENDM
_les	MACRO reg,mem
	mov reg,mem
	ENDM
_mov	MACRO a,b
	ENDM
_inc	MACRO a
	ENDM
_add	MACRO a,b
	ENDM
s_inc	MACRO a
	inc a
	ENDM
s_mov	MACRO a,b
	mov a,b
	ENDM
s_push	MACRO a
	push a
	ENDM
s_pop	MACRO a
	pop a
	ENDM
s_	MACRO a
	a
	ENDM
l_	MACRO a
	ENDM
ENDIF

	; _call: かならず同一セグメントにあると判っているルーチンをcallする
	; ときに使用するマクロ (94/1/1追加)
IF RETSIZE EQ 2
_call	MACRO place
	push	CS
	call	near ptr place
	ENDM
ELSE
_call	MACRO place
	call	near ptr place
	ENDM
ENDIF

.list
