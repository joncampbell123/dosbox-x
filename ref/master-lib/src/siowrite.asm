; master library - PC98
;
; Description:
;	送信バッファにデータを書き込む
;	
;
; Function/Procedures:
;	unsigned sio_write( int ch, const void * senddata, unsigned sendlen ) ;
;
; Parameters:
;	ch	 送信ポート(0=本体内蔵)
;	senddata 書き込むデータの先頭アドレス
;	sendlen	 書き込むデータの長さ(バイト数)
;
; Returns:
;	書き込んだバイト数
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
;	送信バッファが残り少ないときはできるかぎり格納し、sendlenより
;	少ない値を返す場合があります。
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
;	93/ 3/ 8 large data model bugfix
;	93/ 7/23 [M0.20] chをつけただけ
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

func SIO_WRITE
	push	BP
	mov	BP,SP
	; 引数
	channel = (RETSIZE+2+DATASIZE)*2
	senddata= (RETSIZE+2)*2
	sendlen	= (RETSIZE+1)*2

	mov	CX,[BP+sendlen]

	mov	AX,SENDBUF_SIZE	; 送信バッファの残りバイト数
	sub	AX,sio_SendLen

	sub	AX,CX	; AX = min(AX,CX) (書き込む長さを算出)
	sbb	DX,DX
	and	AX,DX
	add	AX,CX
	jz	short WRITE_ZERO	; 書き込まないなら終り

	push	SI
	push	DI
	mov	BX,DS
	mov	ES,BX

	mov	DX,AX			; AX=DX に長さが入る

	mov	BX,sio_send_wp
	lea	DI,[BX+sio_send_buf]
	mov	CX,SENDBUF_SIZE
	sub	CX,BX			; CX = バッファの直線の残り

	sub	CX,DX	; CX = min(CX,DX)
	sbb	BX,BX	;(１回目の書き込みの長さを算出)
	and	CX,BX
	add	CX,DX

	_push	DS
	_lds	SI,[BP+senddata]
	sub	DX,CX			; DX = 残り長さ
	rep	movsb			; １回目転送
	jz	short WRITE_SKIP2
	mov	CX,DX
	mov	DI,offset sio_send_buf
	rep	movsb			; ２回目の転送
WRITE_SKIP2:
	_pop	DS
	add	sio_SendLen,AX

	sub	DI,offset sio_send_buf
	cmp	DI,SENDBUF_SIZE		; 右端(ってどこ)の補正
	sbb	DX,DX
	and	DI,DX
	mov	sio_send_wp,DI		; バッファのポインタを更新

	pushf
	CLI
	cmp	sio_send_stop,1		; 止められてないなら、
	jle	short WRITE_DONE
	mov	DX,AX
	mov	AL,SIO_TXRE*2+1		; 送信割り込み許可だ
	out	SYSTEM_CTRL,AL
	mov	sio_send_stop,0
	mov	AX,DX
WRITE_DONE:
	popf
	pop	DI
	pop	SI

WRITE_ZERO:
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
