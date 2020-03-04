; master library - MS-DOS - 30BIOS
;
; Description:
;	３０行BIOS ( (c)lucifer ) 制御
;	GDCパラメータ設定値取得
;
; Procedures/Functions:
;	int bios30_getparam( int line, struct bios30param * param );
;
; Parameters:
;	line	設定するパラメータを得たい画面行数(行間なし時)
;		0 のときは、現在の値を得る。
;	param	取得するパラメータ値の格納先
;
;	struct bios30param {
;		char hs ;
;		char vs ;
;		char vbp ;
;		char vfp ;
;		char hbp ;
;		char hfp ;
;	} ;
;
; Returns:
;	0 = 機能が使えない
;	1 = 成功
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
;	30bios API 1.10以上が存在すれば、常に動作します。
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
;	93/ 4/25 bios30_setline/getlineをb30line.asmに分離
;	93/ 4/25 bios30_getversionをb30ver.asmに分離
;	93/ 4/25 Initial: master.lib 0.16/b30getmo.asm
;	93/ 9/13 [M0.21] 30bios_exist -> 30bios_tt_exist
;	93/ 9/29 Initial: b30getpa.asm/master.lib 0.21
;

	.MODEL SMALL
	include func.inc
	EXTRN BIOS30_TT_EXIST:CALLMODEL

	.CODE
func BIOS30_GETPARAM	; bios30_getparam( line, param ) {
	push	BP
	mov	BP,SP
	push	SI	; 30bios APIがsi,diを破壊する
	push	DI

	; 引数
	line  = (RETSIZE+1+DATASIZE)*2
	param = (RETSIZE+1)*2

	call	BIOS30_TT_EXIST
	jc	short GET_FAILURE	; 30bios APIがなければ失敗

	mov	BL,[BP+line]
	test	BL,BL
	jnz	short DO_GET
	mov	AX,0ff03h	; 行数取得
	mov	BL,0
	int	18h
	inc	AL		; AL = 行間なしのときの行数
	mov	BL,AL
DO_GET:
	mov	AX,0ff06h
	int	18h
	cmp	AX,0ff06h
	je	short	GET_FAILURE
	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:054dh,4
	jnz	short GDC5MHz
	shr	AL,1
	shr	DL,1
	shr	DH,1
GDC5MHz:
	dec	AL
	dec	DL
	dec	DH
	_push	DS
	_lds	BX,[BP+param]
	mov	[BX+0],AX
	mov	[BX+2],CX
	mov	[BX+4],DX
	_pop	DS

	mov	AX,1
	clc
EXIT:
	pop	DI
	pop	SI
	pop	BP
	ret	(1+DATASIZE)*2

GET_FAILURE:
	xor	AX,AX
	stc
	jmp	short EXIT
endfunc			; }

END
