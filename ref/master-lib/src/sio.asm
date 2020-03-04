; master library - PC98
;
; Description:
;	RS-232C処理のための変数定義
;
; Variables:
;	sio_send_buf	送信バッファ
;	sio_receive_buf	受信バッファ
;	sio_cmdbackup	コマンドレジスタの現在の値の保持
;	sio_send_rp	送信バッファ読み込みポインタ
;	sio_send_wp	送信バッファ書き込みポインタ
;	sio_SendLen	送信バッファ内のデータ長
;	sio_receive_rp	受信バッファ読み込みポインタ
;	sio_receive_wp	受信バッファ書き込みポインタ
;	sio_ReceiveLen	受信バッファ内のデータ長
;	sio_flow_type	フロー制御の方法
;	sio_send_stop	送信不老制御フラグ
;	sio_rec_stop	受信浮浪制御フラグ
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
;

	.MODEL SMALL

	include func.inc

DATADEF equ 1

	include sio.inc

	.DATA?

	public sio_send_buf
sio_send_buf	db	SENDBUF_SIZE dup (?)
	public sio_receive_buf
sio_receive_buf	db	RECEIVEBUF_SIZE dup (?)
	public sio_cmdbackup
sio_cmdbackup	dw	?	; コマンドレジスタのバックアップ


	public sio_send_rp
sio_send_rp	dw	?
	public sio_send_wp
sio_send_wp	dw	?
	public _sio_SendLen,sio_SendLen
_sio_SendLen label word
sio_SendLen	dw	?

	public sio_receive_rp
sio_receive_rp	dw	?
	public sio_receive_wp
sio_receive_wp	dw	?
	public _sio_ReceiveLen, sio_ReceiveLen
_sio_ReceiveLen label word
sio_ReceiveLen	dw	?

	public sio_flow_type
sio_flow_type	dw	?	; フロー制御の方式

	public sio_send_stop
sio_send_stop	dw	?	; 送信フロー制御 0 = 送信可,1=停止, 2=空
	public sio_rec_stop
sio_rec_stop	dw	?	; 受信フロー制御 0 = 受信可, 1 = 停止中
				;		 XON,XOFF = XON/XOFF要求
				;  0にするのは非割り込み, 1にするのは割り込み


END
