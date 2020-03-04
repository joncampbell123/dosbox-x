; master library - 9821
;
; Description:
;	9821拡張グラフィックモードの設定／取得
;
; Function/Procedures:
;	unsigned graph_extmode( unsigned modmask, unsigned bhal ) ;
;
; Parameters:
;	modmask:上位8bit: BHを操作するビットマスク
;	modmask:下位8bit: ALを操作するビットマスク
;	bhal	上位8bit: BHに設定する値(modmaskに対応したビットのみ有効)
;		下位8bit: ALに設定する値(modmaskに対応したビットのみ有効)
;
;		ビットの内容:
;			AL  b7 b6 b5 b4 b3 b2 b1 b0
;						 ++- 0 ノンインターレース
;						     1 インターレース
;					++-++------- 00 15.98kHz
;						     10 24.83kHz
;						     11 31.47kHz
;			BH  b7 b6 b5 b4 b3 b2 b1 b0
;				  || ||	      ++-++- 00 20行
;				  || ||		     01 25行
;				  || ||		     10 30行
;				  ++-++------------- 00 640x200(UPPER)
;						     01 640x200(LOWER)
;						     10 640x400
;						     11 640x480
;	
;
; Returns:
;	実際に設定した値(almod=bhmod=0ならば取得した値)
;	(上位8bit=BH, 下位8bit=ALね)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801/9821, ただし9821でないと実行しても何もしないで 0 を返す。
;
; Requiring Resources:
;	CPU: V30
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
;	恋塚昭彦
;
; Revision History:
;	94/ 1/ 8 Initial: grpextmd.asm/master.lib 0.22

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextShown:WORD

	.CODE

func GRAPH_EXTMODE	; graph_extmode() {
	xor	AX,AX
	mov	ES,AX
	test	byte ptr ES:[045ch],40h
	jz	short G31_NOTMATE
	;
	modmask = (RETSIZE+1)*2
	bhal = (RETSIZE+0)*2
	mov	BX,SP
	mov	CX,SS:[BX+modmask]
	mov	DX,SS:[BX+bhal]
	mov	AH,31h
	int	18h		; 拡張グラフアーキテクチャモードの取得
	mov	AH,BH
	jcxz	short EXT_DONE	; modmask=0だったら読み取って終わり
	and	DX,CX
	not	CX
	and	AX,CX
	or	AX,DX
	mov	CX,AX
	mov	BH,AH

	mov	AH,30h		; 拡張グラフアーキテクチャモードの設定
	int	18h

	test	TextShown,1
	jz	short TEXT_SHOWN
	mov	AH,0ch
	int	18h		; テキスト画面の表示
TEXT_SHOWN:
	test	CL,1
	jz	short NO_SETAREA
	mov	AH,0eh		; 一つの表示領域の設定
	xor	DX,DX
	int	18h
NO_SETAREA:
	test	byte ptr ES:[0711h],1
	jz	short CURSOR_HIDDEN
	mov	AH,11h		; カーソルの表示
	int	18h
CURSOR_HIDDEN:
	mov	AX,CX
EXT_DONE:
G31_NOTMATE:
	ret	4
endfunc		; }

END
