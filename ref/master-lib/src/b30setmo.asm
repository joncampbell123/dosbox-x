; master library - MS-DOS - 30BIOS
;
; Description:
;	30行BIOS ( (c)lucifer ) 動作制御
;
; Function/Procedures:
;	void bios30_setmode( int mode );
;		mode ... BIOS30_NORMAL, BIOS30_SPECIAL, BIOS30_RATIONAL, BIOS30_VGA
;			 BIOS30_FKEY, BIOS30_CW
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with 30bios or TT)
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	30bios/TT APIが存在すれば動作します。存在しなければなにもしません。
;	30行BIOSでRationalモード未サポートの場合は設定が無効になる。
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
;	ちゅ～た
;
; Advice & Check:
;	さかきけい
;
; Revision History:
;	93/ 4/10 Initial: master.lib/b30.asm
;	93/ 4/25 bios30_setline/getlineをb30line.asmに分離
;	93/ 4/25 bios30_getversionをb30ver.asmに分離
;	94/ 4/25 bios30_getmodeをb30getmo.asmに分離
;	93/ 8/30 [M0.21] TT 1.0に対応… TTの古いバージョンでは誤動作する?
;	93/ 9/13 [M0.21] bios30_exist廃止, bios30_tt_exist追加
;	93/ 9/13 Initial: b30setmo.asm/master.lib 0.21
;	93/12/ 9 [M0.22] TT API対応
;	94/ 8/ 8 30行BIOS Ver1.20β27（現在）に対応

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
;		mode ... {BIOS30_NORMAL,BIOS30_SPECIAL,BIOS30_RATIONAL,BIOS30_VGA}
;			|{BIOS30_FKEY, BIOS30_CW}
;			|{BIOS30_WIDELINE, BIOS30_NORMALLINE}
; 上位バイトはmodify maskでーす
; ただし、0のときは下位バイトを直接設定しますよ。
; 30biosが存在しなければ何もしません。
func BIOS30_SETMODE	; void bios30_setmode( int mode ) {
	_call	BIOS30_TT_EXIST
	and	AL,84h			; 30bios API, TT 1.50 API
	mov	BX,SP
	mode = (RETSIZE+0)*2
	mov	AX,SS:[BX+mode]
	jz	short SET_FAILURE
	jns	short SET_TT

	mov	BX,'30'+'行'
	not	AH
	test	AH,AH
	je	short SET_NOMODIFY
	mov	CX,AX
	mov	AH,0bh	; getmode
	int	18h
	and	AL,CH
	or	AL,CL
SET_NOMODIFY:
	mov	AH,0ah
TT_RETURN:
	int	18h
SET_FAILURE:
	ret	2

;		30bios   → 1.20			TT
;		0..0.... 00.0.... normal		.00.0...
;		0..1.... 00.1.... special (View)	.01.1...
;		1..1.... 11.1.... vga     (All)		.10.1...
;		........ 01.1.... rational(Layer)	.00.1...
;
;		..0..... ..0..... function key		........
;		..1..... ..1..... CW			........
;
;		.......0 .......0 normalline		.......0
;		.......1 .......1 wideline		.......1

;
; 以下の手順は、bit7(vga)だけを落とそうとして(->special)、8000hが渡されても
; normal modeに設定してしまう問題がある。
; normal modeのときに実行したのならいいのだが。まあ、そのときの動作は
; 30biosでは定義されてないので、9010hで設定してもらわないと困るからいいか。
;
SET_TT:
	mov	dx,ax		; dx モード
	and	dx,0101h

	and	ax,0d0d0h	; 画面状態をTT用にする
	jz	short TT_NO_MODE
	shr	ax,1
	xor	al,20h
	or	dx,ax
;	test	dl,40h		; | 定義されている値が前提
;	jz	TT_NO_ALL	; | 無効な画面状態が設定される可能性があるなら
;	and	dl,NOT 20h	; | ここのコメントをはずす
TT_NO_ALL:
TT_NO_MODE:
	mov	bx,'TT'		; モード設定
	mov	ax,1802h
	int	18h
	not	dh
	and	al,dh
	or	al,dl
	jmp	short TT_RETURN
endfunc			; }

END
