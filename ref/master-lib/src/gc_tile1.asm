PAGE 98,120
; graphics - grcg - tile - 1line
;
; Description:
;	タイルパターンの設定(1 line)
;
; Function/Procedures:
;C:	void [_far] _pascal grcg_settile_1line( int mode, long tile ) ;
;
;Pas:	procedure grcg_settile_1line( mode : integer, tile : longint ) ;

; Parameters:
;	int mode	GRCGのモード。
;	long tile	タイルパターン。下位バイトから１バイト単位に
;			B,R,G,Iプレーンとなる。
;			0x000000FFLが、青プレーンのみ全てON,それ以外
;			全てOFFの値となる。
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRCG
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6 ( MASM 5.0互換ならば OK )
;
; NOTES:
;
; AUTHOR:
;	恋塚昭彦
;
; 関連関数:
;	grcg_setcolor()
;	grcg_off()
;
; HISTORY:
;	92/ 6/28 Initial

	.186
	.MODEL SMALL
	.CODE
	include func.inc

func GRCG_SETTILE_1LINE
	mov	CX,BP
	mov	BP,SP

	; parameters
	mode  = (retsize+2)*2
	tileh = (retsize+1)*2
	tilel = (retsize+0)*2

	mov	AL,[BP+mode]
	out	7ch,AL
	mov	DX,007eh

	mov	AX,[BP+tilel]
	out	DX,AL		; B
	mov	AL,AH
	out	DX,AL		; R

	mov	AX,[BP+tileh]
	out	DX,AL		; G
	mov	AL,AH
	out	DX,AL		; I

	mov	BP,CX
	ret	6
endfunc

END
