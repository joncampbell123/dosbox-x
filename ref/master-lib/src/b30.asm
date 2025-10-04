; master library - MS-DOS - 30BIOS
;
; Description:
;	30行BIOS ( (c)lucifer ) 制御
;	TT ( (C)早紀 ) の存在検査
;
; Procedures/Functions:
;	bios30_tt_exist();
;	bios30_push();
;	bios30_pop();
;
; Parameters:
;	
;
; Returns:
;	bios30_tt_exist:
;		00h = 不在
;		01h = ?
;		02h = TT(0.70～0.80; 30biosエミュレーションなし)が存在
;		03h = ?
;		06h = TT(1.50)が存在
;		80h = 30bios 0.20未満
;		81h = 30bios 0.20以降
;		82h = TT(1.00)が存在
;		83h = TT(0.90または1.05)が存在
;		89h = 30bios 1.20以降
;
;		すなわち、
;		bit 1が 1なら TTが存在する
;		2 ならすごく古い(笑) TT が存在する
;		80h以上なら 30bios APIが存在する
;		81h以上なら、30bios 0.20以降のAPIが存在する
;		ってことになるのであーる
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
;	bios30_push, bios30_pop は、30biosのバージョンが 0.07よりも古いと
;			何も起こりません。
;			TTのバージョンが 1.50より古いときも何もしません。
;
;
; Assembly Language Note:
;	30bios_tt_exist の終了値の本当の意味は下の通り
;	  AL の値
;	    bit 0: 1=厳密なチェック通過
;	    bit 1: 1=TT API存在
;	    bit 2: 1=TT 1.50以降
;	    bit 3: 1=30bios 1.20以降
;	    bit 7: 1=30bios API存在
;	  z flag : AL = 0のとき 1
;	  c flag : (AL and 80h) = 0(=30bios APIなし) のとき 1
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
;	93/12/ 9 [M0.22] TT 1.50に対応
;	94/ 8/ 8 30行BIOS Ver1.20β27（現在）に対応
;

	.MODEL SMALL
	include func.inc

	.DATA
check_string	db '30BIOS_EXIST='
check_byte	db '0'

	.CODE

func BIOS30_TT_EXIST	; bios30_tt_exist(void) {
	push	DI

CHECK_30BIOS:
	mov	check_byte,'0'
	mov	AH,0bh
	mov	BX,'30'+'行'
	push	DS
	pop	ES
	mov	DI,offset check_string
	int	18h
	shl	AL,1		; bit 6 -> bit 7
	and	AL,80h
	jz	short NO_BIOS30	; 30行BIOSのAPIがあるならバージョンを取得
	mov	dl,al
	mov	ax,0ff00h
	int	18h
	xchg	ah,al
	cmp	ax,(1 SHL 8) OR 20
	cmc
	sbb	al,al
	and	al,08h		; bit 3 30BIOS 1.20以降
	or	al,dl
NO_BIOS30:
	or	check_byte,AL

CHECK_TT:
	mov	BX,'TT'		; TT 0.70～ Install Check
	mov	AX,1800h
	int	18h
	sub	AX,BX		; if exist: AX=0
	cmp	AX,1
	sbb	AX,AX
	jz	short NO_TT

	mov	BX,'TT'		; TTがあるので、 1.50以降かどうか判定
	mov	AX,1810h
	int	18h
	shl	AH,1		; 1.50以降はMSBが立っている
	sbb	AX,AX
	and	AX,4		; それが 0 でなければ 1.50以降あり
	or	AL,2		; いずれにせよ TT API あり
	or	check_byte,AL

NO_TT:
	mov	AL,check_byte
	and	AL,8fh	; 0なら zf=1
	rol	AL,1
	ror	AL,1
	cmc		; bit 7=0なら cy=1

	pop	DI
	ret
endfunc			; }

func BIOS30_PUSH	; bios30_push(void) {
	call	BIOS30_TT_EXIST
	and	AL,84h	; 30bios API, TT 1.50 api
	jz	short PUSH_FAILURE
	mov	AX,0ff01h	; 30bios API push
	js	short PUSH_30BIOS
	mov	BX,'TT'
	mov	AX,180ah	; TT 1.50 API push
PUSH_30BIOS:
	int	18h
	and	AL,0fh	; -1, 1～15が成功。0,16はエラー
	cmp	AL,1
	sbb	AX,AX
	inc	AX	; success=1  failure=0
PUSH_FAILURE:
	ret
endfunc			; }

func BIOS30_POP		; bios30_pop(void) {
	call	BIOS30_TT_EXIST
	and	AL,84h	; 30bios API, TT 1.50 api
	jz	short POP_FAILURE
	mov	AX,0ff02h	; 30bios API pop
	js	short POP_30BIOS
	mov	BX,'TT'
	mov	AX,180bh	; TT 1.50 API pop
POP_30BIOS:
	int	18h	; 30bios: AX; -1=succes, 0=failure
			; TT1.50: AL; 15～0=success, -1=failure
	and	AH,AL
	add	AH,1
	sbb	AH,AH	; これで 30bios success= 0ffxxh, else 00xxh
	not	AL	;        TT1.50: failure=0000h
	cmp	AX,1
	sbb	AX,AX
	inc	AX	; success=1  failure=0
POP_FAILURE:
	ret
endfunc			; }

END
