; master library - PC98
;
; Description:
;	ビープ音の周波数の設定
;
; Function/Procedures:
;	void beep_freq( unsigned freq ) ;
;
; Parameters:
;	unsigned freq	周波数(38～65535Hz), 既定値は 2000Hz
;
; Returns:
;	none
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
;	範囲外の値を入力すると、０除算実行時エラーが発生するので
;	注意してください。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial
;	93/ 6/25 [M0.19] 少し縮小
;	93/ 9/15 [M0.21] 5MHz/8MHz系判定を BIOSワーク読み取りに変更
;	93/11/30 [M0.22] AT
;	94/ 4/11 [M0.23] AT用を vbeepfrq.asmに分離


	.MODEL SMALL

	include func.inc

	.CODE

func BEEP_FREQ	; beep_freq() {
	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h

	mov	BX,SP

	; 引き数
	freq = RETSIZE*2

	mov	DX,001eh 	; 1996.8K(8MHz系)
	mov	AX,7800h

	jnz	short EIGHTMEGA

	mov	DX,0025h	; 2457.6K(10MHz系)
	mov	AX,8000h

EIGHTMEGA:
	div	word ptr SS:[BX+freq]		; AX = setdata
	mov	DX,3fdbh

	out	DX,AL
	mov	AL,AH
	out	DX,AL
	ret	2
endfunc		; }

END
