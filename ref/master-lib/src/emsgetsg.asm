; master library - EMS - GETSEGMENT
;
; Description:
;	物理ページのセグメントアドレスの配列の取得
;
; Function/Procedures:
;	int ems_getsegment( unsigned * segments, int maxframe )
;
; Parameters:
;	segments	物理ページセグメントの配列の格納先
;	maxframe	格納可能な最大数
;
; Returns:
;	0		エラー
;	1～		格納できたセグメントの数
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;	EMS: LIM EMS 4.0
;
; Notes:
;	結果、segmentsの配列添え字が物理ページ番号、その内容がセグメント
;	アドレスになります。
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
;	93/ 7/19 Initial: emsgetsg.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.CODE

func EMS_GETSEGMENT	; ems_getsegment() {
	push	SI
	push	DI
	push	BP
	mov	BP,SP

	; 引数
	segments = (RETSIZE+4)*2
	maxframe = (RETSIZE+3)*2

	mov	AX,5801h	; エントリ数取得
	int	67h
	test	AH,AH
	mov	AX,0
	jne	short ERROR_EXIT

	push	SS
	pop	ES
	mov	AX,CX
	shl	AX,1
	shl	AX,1
	sub	SP,AX
	mov	DI,SP	; ES:DI= 格納先
	mov	AX,5800h
	int	67h
	test	AH,AH
	mov	AX,0
	jne	short ERROR_EXIT

	mov	SI,DI
    s_ <push	DS>
    s_ <pop	ES>
	_les	DI,[BP+segments]

	mov	AX,[BP+maxframe]
	cmp	AX,CX
	jl	short TOBI	; cxは実際の個数
	mov	AX,CX		; axは min( 指定数, 実際の個数 )
TOBI:
	test	AX,AX
	jz	short ERROR_EXIT ; 0個ならおわりだーこのやろ

	xor	DX,DX
NLOOP:
	mov	CH,CL
	xor	BX,BX
ILOOP:
	cmp	SS:[SI+BX+2],DX
	je	short FOUND
	add	BX,4
	dec	CH
	jge	short ILOOP
	xor	BX,BX		; 見つからないときは最初のエントリで代用(ｹﾞ)!
FOUND:
	mov	BX,SS:[SI+BX]
	mov	ES:[DI],BX
	inc	DI
	inc	DI

	inc	DX
	cmp	DX,AX
	jb	short NLOOP

ERROR_EXIT:
	mov	SP,BP
	pop	BP
	pop	DI
	pop	SI
	ret	(1+DATASIZE)*2
endfunc		; }

END
