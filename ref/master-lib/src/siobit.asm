; master library - PC98
;
; Description:
;	RS-232Cの制御線の操作
;
; Function/Procedures:
;	void sio_bit_on( int ch, int mask ) ;
;	void sio_bit_off( int ch, int mask ) ;
;
; Parameters:
;	ch	0	  本体内蔵ポート
;	mask	SIO_ER	  DTR信号を制御します。
;		SIO_RS	  RTS信号を制御します。
;		SIO_BREAK ブレーク信号を制御します。
;
; Returns:
;	none
;
; Assembly Language Note:
;	レジスタは、ALレジスタのみ破壊します。
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
;	93/ 7/23 [M0.20] ポート番号をつけた(だけ)
;	93/ 9/21 [M0.21] sio_bit_offが死んでた
;
	.MODEL SMALL
	include func.inc
	include sio.inc
	.CODE

func SIO_BIT_ON		; sio_bit_on() {
	push	BP
	mov	BP,SP
	mov	AL,[BP+(RETSIZE+1)*2]	; 引数
	and	AL,10111111b
	pushf
	CLI
	or	AL,byte ptr sio_cmdbackup
	out	SIO_CMD,AL
	mov	byte ptr sio_cmdbackup,AL
	popf
	pop	BP
	ret	4
endfunc			; }

func SIO_BIT_OFF	; sio_bit_off() {
	push	BP
	mov	BP,SP
	mov	AL,[BP+(RETSIZE+1)*2]	; 引数
	and	AL,10111111b
	pushf
	CLI
	not	AL
	and	AL,byte ptr sio_cmdbackup
	out	SIO_CMD,AL
	mov	byte ptr sio_cmdbackup,AL
	popf
	pop	BP
	ret	4
endfunc			; }

END
