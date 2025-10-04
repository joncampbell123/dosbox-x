; master library - PC98
;
; Description:
;	送信バッファに１文字追加
;
; Function/Procedures:
;	int sio_putc( int port, int c ) ;
;
; Parameters:
;	port = 送信先ポート番号
;	c = 送信したい１バイトのデータ
;
; Returns:
;	int	1 = 成功
;		0 = バッファが満杯
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	92/12/07 Initial
;	93/ 7/23 [M0.20] 引数にportを足しただけ
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

func SIO_PUTC
	push	BP
	mov	BP,SP

	; 引数
	port	= (RETSIZE+2)*2
	c	= (RETSIZE+1)*2

	mov	CX,[BP+c]

	xor	AX,AX
	cmp	sio_SendLen,SENDBUF_SIZE
	je	short PUTC_FULL		; 送信バッファが満杯なら失敗

	mov	BX,sio_send_wp
	mov	[BX+sio_send_buf],CL	; 送信バッファに追加
	RING_INC BX,SENDBUF_SIZE,AX
	mov	sio_send_wp,BX		; バッファのポインタを更新する
	inc	sio_SendLen

	pushf
	CLI
	cmp	sio_send_stop,1		; 止められてないなら、
	jle	short PUTC_DONE
	mov	AL,SIO_TXRE*2+1		; 送信割り込み許可だ
	out	SYSTEM_CTRL,AL
	mov	sio_send_stop,0

PUTC_DONE:
	popf
	mov	AX,1
PUTC_FULL:
	pop	BP
	ret	4
endfunc

END
