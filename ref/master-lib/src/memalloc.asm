; master library - MSDOS
;
; Description:
;	メインメモリの上位からの確保
;
; Function/Procedures:
;	unsigned mem_allocate( unsigned para ) ;
;
; Parameters:
;	unsigned para	確保するパラグラフサイズ
;
; Returns:
;	unsigned 確保されたDOSメモリブロックのセグメント (cy=0)
;		 0 ならメモリが足りないので失敗でやんす (cy=1)
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	DOS: 3.0 or later
;
; Notes:
;	AX以外のレジスタ保護
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/ 2 Initial
;	92/12/11 ゲ、DOS呼んでないじゃん
;	93/ 1/31 mem_freeを dosfree.asmに移動

	.MODEL SMALL
	include func.inc
	.CODE

func MEM_ALLOCATE
	push	BX
	push	CX

	mov	AX,5800h	; アロケーションストラテジを得る
	int	21h
	push	AX		; 得たストラテジを保存する

	mov	AX,5801h	;
	mov	BX,2		; 最上位からのアロケーション
	int	21h

	mov	BX,SP
	mov	BX,SS:[BX+(RETSIZE+3)*2]	; para
	mov	AH,48h		; 確保する
	int	21h
	cmc
	sbb	CX,CX
	and	CX,AX		; CX=seg, 失敗なら 0

	mov	AX,5801h	; アロケーションストラテジの復帰
	pop	BX		;
	int	21h		;

	mov	AX,CX
	cmp	AX,1		; 失敗は cy=1
	pop	CX
	pop	BX
	ret	2
endfunc

END
