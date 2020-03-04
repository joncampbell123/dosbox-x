; master library - PC98
;
; Description:
;	送信バッファに文字列を書き込む
;
; Function/Procedures:
;	unsigned sio_puts( int port, char void * sendstr ) ;
;
; Parameters:
;	port	送信するポート(0=本体内蔵)
;	sendstr 書き込む文字列
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
;	93/ 2/ 4 Initial
;	93/ 7/23 [M0.20] port追加
;
	.MODEL SMALL

	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_WRITE:CALLMODEL

func SIO_PUTS
	push	BP
	mov	BP,SP
	push	DI

	; 引数
	port	= (RETSIZE+1+DATASIZE)*2
	sendstr	= (RETSIZE+1)*2

	push	[BP+port]
  s_	<mov ax,ds>
  s_	<mov es,ax>
	_les	DI,[BP+sendstr]
	_push	ES	; parameter to sio_write
	push	DI

	mov	CX,-1
	mov	AL,0
	repne	scasb
	not	CX
	dec	CX	; CX = len(sendstr)

	push	CX
	call	SIO_WRITE

	pop	DI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
