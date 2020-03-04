; 区間の消費時間を測定した時間を文字列に変換する
;
; Function:
;	void perform_str( char * buf, unsigned long perform ) ;
;
; Parameters:
;	char; buf		書き込み先。
;	unsigned long perform	perform_stop()の戻り値
;
; Description:
;	perform_stop()によって得た、内部消費時間を文字列に変換する。
;	bufに時間を、11文字の実数文字列として格納する。
;	格納する書式は、str_printfの "%6ld.%04ld"の形。
;	"1.0000"が１ミリ秒を表す。
;
; Prototype:
;	#include "perform.h"
;
; 参照関数:
;	perform_start()
;	perform_stop()
;
; 対象機種:
;	NEC PC-9801series
;	PC/AT
;
; 計測精度:
;	refer perform_start()
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/5/18 Initial(perf_str.c)
;	92/5/25 perform_start/stopをperforma.asmに移動
;	92/12/19 perfstr.asm ( master.lib )
;	93/ 5/29 [M0.18] .CONST->.DATA
;	93/12/ 7 [M0.22] add variable perform_Rate
;
	.MODEL SMALL
	include func.inc
	EXTRN _str_printf:CALLMODEL

	.DATA
	EXTRN perform_Rate:WORD
formatstr db '%6ld.%04ld',0

	.CODE

; DXAX / CX = DXAX ... BX
; 92/12/19
	public ASM_LWDIV
ASM_LWDIV	proc near
LLONG:
	xor	BX,BX
	test	DX,DX		; ←この２行を取れば4bytes減るが
	je	short LWORD	; ←それは避けたい…
	xchg	BX,AX	; BX <- AX ( 被除数の下位 )
	xchg	AX,DX	; AX <- DX ( 被除数の上位 )
			; DX = 0
	div	CX
	xchg	AX,BX	; BX = 解の上位16bit
LWORD:
	div	CX
	xchg	DX,BX	; BX = 余り
	ret
	EVEN
ASM_LWDIV	endp

func PERFORM_STR
	push	BP
	mov	BP,SP
	push	SI
	; 引数
	buf	= (RETSIZE+3)*2
	perform = (RETSIZE+1)*2

	mov	CX,perform_Rate
	shr	CX,1
	mov	AX,[BP+perform]
	mov	DX,[BP+perform+2]

	mov	SI,AX	; DXAXを5倍
	mov	BX,DX
	shl	AX,1
	rcl	DX,1
	shl	AX,1
	rcl	DX,1
	add	AX,SI
	adc	DX,BX

	call	ASM_LWDIV	; tim / rate
	push	DX
	mov	SI,AX

	mov	AX,10000	; tim % rate; 10000 / rate
	mul	BX
	call	ASM_LWDIV
	pop	BX
	push	DX
	push	AX
	push	BX
	push	SI

	_push	DS
	mov	AX,offset formatstr
	push	AX
	_push	[BP+buf+2]
	push	[BP+buf]

	call	_str_printf
	add	SP,(4+DATASIZE*2)*2
	pop	SI
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
