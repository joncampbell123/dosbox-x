; master library - PC-9801 - RS-232C - READ SIGNAL
;
; Description:
;	RS-232Cの各種状態を得る
;
; Function/Procedures:
;	int sio_read_signal( int ch ) ;
;	int sio_read_err( int ch ) ;
;	int sio_read_dr( int ch ) ;
;
; Parameters:
;	ch	RS-232Cポートのチャンネル番号( 0=本体内蔵 )
;
; Returns:
;	sio_read_signal	SIO_CI(80h) 着呼検出
;	sio_read_signal SIO_CS(40h) 送信可
;	sio_read_signal SIO_CD(20h) キャリア検出
;	sio_read_err	SIO_PERR    パリティエラー
;	sio_read_err	SIO_OERR    オーバーランエラー
;	sio_read_err	SIO_FERR    フレーミングエラー
;	sio_read_dr	!=0         DTR検出
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
;	93/ 7/23 Initial: siosignal.asm/master.lib 0.20
;	93/ 9/22 [M0.21] sio_read_signalのビット反転忘れ修正

	.MODEL SMALL
	include func.inc

	.CODE

func SIO_READ_SIGNAL	; sio_read_signal() {
	in	AL,33h
	not	AX
	and	AX,0e0h
	ret	2
endfunc		; }

func SIO_READ_ERR	; sio_read_err() {
	in	AL,32h
	and	AX,38h
	ret	2
endfunc		; }

func SIO_READ_DR	; sio_read_dr() {
	in	AL,32h
	and	AX,80h
	ret	2
endfunc		; }

END
