; master library - MS-DOS - 30BIOS
;
; Description:
;	３０行BIOS ( (c)lucifer ) 制御
;	行数関係
;
; Procedures/Functions:
;	unsigned bios30_getline( void ) ;
;	void bios30_setline( int lines ) ;
;
; Parameters:
;	bios30_setline:
;		line: 行間なしのときの設定行数
;
; Returns:
;	bios30_getline:
;		上位バイト: 行間ありのときの行数-1
;		下位バイト: 行間なしのときの行数-1
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30bios のバージョンが 0.08よりも古いと異常かもしれません(^^;
;	30bios, TT 1.50 API のどちらも常駐していなければ 0 を返します。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 Initial: b30line.asm/master.lib 0.16
;	93/ 9/13 [M0.21] 30bios_exist -> 30bios_tt_exist
;	93/12/ 9 [M0.22] TT API 対応
;	93/12/11 [M0.22] BUGFIX TT APIでのgetlineは値が 1引かれていなかった
;

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETLINE	; unsigned bios30_getline( void ) {
	_call	BIOS30_TT_EXIST
	and	AL,84h
	jz	short GETLINE_FAILURE
	mov	BL,0
	mov	AX,0ff03h		; 30bios API; get line
	js	short GETLINE_30BIOS
	mov	AX,1809h		; TT 1.50 API; Get TextLine2
	mov	BX,'TT'
	int	18h
	sub	AX,0101h		; それぞれ1ずつ引く
	ret
GETLINE_30BIOS:
	int	18h
GETLINE_FAILURE:
	ret
endfunc			; }


func BIOS30_SETLINE	; void bios30_setline( int lines ) {
	_call	BIOS30_TT_EXIST
	and	AL,82h		; 30bios API, TT API
	jz	short SETLINE_FAILURE
	mov	AX,0ff03h	; 30bios API; set line
	mov	BX,SP
	mov	BL,SS:[BX+RETSIZE*2]
	js	short SETLINE_30BIOS

	mov	AL,BL
	cmp	AL,20			; TTで20行未満の設定は別コマンドだし…
	jl	short SETLINE_FAILURE	; ExtMode設定はMSB=1, つまり負なのだ
	mov	AH,18h
	mov	BX,'TT'
SETLINE_30BIOS:
	int	18h
SETLINE_FAILURE:
	ret	2
endfunc			; }

END
