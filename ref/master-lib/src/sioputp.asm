; master library - PC98 - RS-232C - PascalString
;
; Description:
;	送信バッファにパスカル文字列を書き込む
;
; Function/Procedures:
;	unsigned sio_puts( int port, char void * passtr ) ;
;
; Parameters:
;	port	送信するポート(0=本体内蔵)
;	sendstr 書き込むパスカル文字列
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
;	93/ 7/23 Initial: sioputp.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc
	include sio.inc

	.CODE

	EXTRN SIO_WRITE:CALLMODEL

func SIO_PUTP	; sio_putp() {
	push	BP
	mov	BP,SP

	; 引数
	port	= (RETSIZE+1+DATASIZE)*2
	sendstr	= (RETSIZE+1)*2

	push	[BP+port]
	_les	BX,[BP+sendstr]
	_push	ES	; parameter to sio_write
  s_	<mov	AL,[BX]>
  l_	<mov	AL,ES:[BX]>
  	mov	AH,0
	inc	BX
	push	BX
	push	AX
	call	SIO_WRITE

	pop	BP
	ret	(1+DATASIZE)*2
endfunc		; }

END
