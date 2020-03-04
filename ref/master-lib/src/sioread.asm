; master library - PC98
;
; Description:
;	受信バッファからデータを読み込む
;
; Function/Procedures:
;	int sio_getc( int port ) ;
;	unsigned sio_read( int port, void * recbuf, unsigned reclen ) ;
;
; Parameters:
;	
;
; Returns:
;	sio_getc = 0～255: 読み込んだ１バイトのデータ
;		   -1:	受信バッファが空
;	sio_read = 読み込んだバイト数( 0 = 受信バッファ空 )
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
;	93/ 2/19 request_xon()の割り込み禁止区間を減らした
;	93/ 7/23 [M0.20] 引数port追加。中身はまだない。
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_BIT_ON:CALLMODEL


; 受信バッファに溜っているデータが一定以下に減ったときに、
; 通信相手の送信許可を出す
; IN:
;	BX = port number ( 0=internal )
REQUEST_XON proc near
	cmp	sio_rec_stop,0
	je	short REQXON_NG
REQXON_OK:
	ret

REQXON_NG:
	; 停止中で、バッファが一定以下に減ったならばむこうに送信許可を…
	cmp	sio_ReceiveLen,FLOW_STOP_SIZE
	jae	short REQXON_OK		; まだ多いからなにもしないぜ

	; むこうへ送信許可を出す
	cmp	sio_flow_type,1		; SIO_FLOW_HARD:1
	ja	short REQXON_SOFT	; SIO_FLOW_SOFT:2
	jb	short REQXON_OK		; SIO_FLOW_NO:0
REQXON_HARD:
	; ハードフロー(RS/CS)
	push	AX
	push	BX			; port
	mov	AX,SIO_CMD_RS		; RS信号をON
	push	AX
	call	SIO_BIT_ON
	pop	AX
	mov	sio_rec_stop,0		; 送信許可だ
	jmp	short REQXON_OK

REQXON_SOFT:
	; ソフトフロー(XON/XOFF)
	CLI
	push	AX
	mov	sio_rec_stop,XON	; XON送信要求を出す
	mov	AL,SIO_TXRE*2+1		; 送信割り込み許可
	out	SYSTEM_CTRL,AL
	pop	AX
	STI
	ret
REQUEST_XON endp


func SIO_GETC
	mov	AX,-1
	cmp	sio_ReceiveLen,0
	je	short GETC_NOBUF		; 受信バッファが空なら失敗

	mov	BX,sio_receive_rp
	xor	AH,AH
	mov	AL,[BX+sio_receive_buf]		; 受信バッファから 1byte取る
	RING_INC BX,RECEIVEBUF_SIZE,DX		; バッファのポインタを更新する
	mov	sio_receive_rp,BX
	dec	sio_ReceiveLen

	mov	BX,SP
	port = (RETSIZE)*2
	mov	BX,SS:[BX+port]
	call	REQUEST_XON
GETC_NOBUF:
	ret	2
endfunc


func SIO_READ
	push	BP
	mov	BP,SP
	; 引数
	port = (RETSIZE+2+DATASIZE)*2
	recbuf	= (RETSIZE+2)*2
	reclen	= (RETSIZE+1)*2

	mov	CX,[BP+reclen]
	mov	AX,sio_ReceiveLen	; 受信バッファのバイト数

	sub	AX,CX	; AX = min(AX,CX) (読み込む長さを算出)
	sbb	DX,DX
	and	AX,DX
	add	AX,CX
	jz	short READ_ZERO		; ０バイトなら無視

	push	SI
	push	DI
	mov	DX,AX			; AX=DX に長さが入る

	mov	BX,sio_receive_rp
	lea	SI,[BX+sio_receive_buf]
	mov	CX,RECEIVEBUF_SIZE
	sub	CX,BX			; CX = バッファの直線の残り

	sub	CX,DX	; CX = min(CX,DX)
	sbb	BX,BX	;(１回目の読み込みの長さを算出)
	and	CX,BX
	add	CX,DX

	s_mov	BX,DS
	s_mov	ES,BX
	_les	DI,[BP+recbuf]
	sub	DX,CX			; DX = 残り長さ
	rep	movsb			; １回目転送
	jz	short READ_SKIP2
	mov	CX,DX
	mov	SI,offset sio_receive_buf
	rep	movsb
READ_SKIP2:
	sub	sio_ReceiveLen,AX

	sub	SI,offset sio_receive_buf
	cmp	SI,RECEIVEBUF_SIZE	; 右端(ってどこ)の補正
	sbb	DX,DX
	and	SI,DX
	mov	sio_receive_rp,SI	; バッファのポインタを更新

	mov	BX,[BP+port]
	call	REQUEST_XON
	pop	DI
	pop	SI

READ_ZERO:
	pop	BP
	ret	(2+DATASIZE)*2
endfunc

END
