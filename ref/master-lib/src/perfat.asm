; 区間の消費時間を測定する[PC/AT]
;
; Function/Procedures:
;	void perform_at_start(void) ;
;	unsigned long perform_at_stop(void) ;
;
; Usage:
;   perform_start()を実行した瞬間から、perform_stop()を実行するまでの
;   時間を測定し、perform_stop()の戻り値として返す。
;   　ただし、この値は内部的なものなので、perform_str()を使って実数文字列に
;   変換して利用すること。
; 
; 対象機種:
; 	PC/AT
; 
; 計測精度:
; 	計測単位: 1/1193.18msec
; 	最長範囲: 174760*2(215089.2308*2)msec  <-mada keisan sitenai
; 		→秒換算で約349(430)秒
;
; 	計測誤差: 実行中にかかる割り込みに影響し、40msec当り0.5msec程度…。
; 
; Notes:
; 	タイマ割り込みを使用するプログラムとは共存できません。
; 	（既に使われているタイマ割り込みは停止してしまいます）
;
; 実装について:
;	タイマ割り込みのモードをワンショットにしている理由ですが、
;	これは実際にアプリケーションが消費した時間を計るために
;	計時のオーバーヘッドを計測範囲から追い出すために行った処置です。
;
; COMPILER/ASSEMBLER:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
; 	恋塚昭彦
; 
; Revision History:
; 	92/5/25 Initial ( ｺﾝﾊﾟｲﾗ依存を減らすため、perform.cより分離 )
;	92/6/17 TASM, Pacsal対応
;	92/11/21 master.libに追加
;	92/12/20 perform_stop()でタイマ#0をmode 3,10msに設定するようにした
;	93/ 8/ 4 [M0.20] ベクタ復帰が出来てなかったのを bugfix(^^;
;	93/12/ 7 Initial: perfat.asm/master.lib 0.22
;

	.MODEL SMALL

	.DATA?

	EXTRN perform_Rate:WORD
LASTTINT	dd	?
TIMER		dw	?

	.CODE
	include func.inc

; タイマ割り込みのエントリ
timer_entry	PROC	FAR
	push	AX
	push	DS
	mov	AX,seg TIMER
	mov	DS,AX
	inc	TIMER
	pushf
	call	LASTTINT	; includes EOI
	pop	DS
	mov	AL,30h
	out	43h,AL		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	mov	AL,0FFh
	out	40h,AL		; counter LSB
	out	40h,AL		; counter MSB, counter start
	pop	AX
	iret
	EVEN
timer_entry	ENDP

func PERFORM_AT_START	; perform_at_start() {
	xor	AX,AX
	mov	TIMER,AX
	CLI
	mov	DX,21h
	in	AL,DX
	or	AL,1		; MASK!
	out	DX,AL
	mov	AL,0
	STI

	mov	AX,3508h	; 割り込みベクタ08hの読み取り
	int	21h
	mov	word ptr LASTTINT,BX
	mov	word ptr LASTTINT+2,ES

	push	DS
	mov	DX,offset timer_entry
	mov	AX,seg timer_entry
	mov	DS,AX
	mov	AX,2508h	; 割り込みベクタ08hの書き込み
	int	21h
	pop	DS

	CLI

	mov	AL,30h		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	out	43h,AL

	mov	AL,0FFh
	out	40h,AL		; counter LSB
	out	40h,AL		; counter MSB, counter start

	mov	DX,21h
	in	AL,DX
	and	AL,0FEh		; Clear Mask
	out	DX,AL

	STI
	ret
	EVEN
endfunc			; }


;
; unsigned long _far perform_stop(void) ;
; function perform_stop : Longint ;
FUNC PERFORM_AT_STOP	; perform_at_stop() {
	CLI
	mov	AL,0	; latch
	out	043h,AL

	mov	DX,40h
	in	AL,DX
	mov	AH,AL
	in	AL,DX
	xchg	AH,AL	; w = inp(0x40) + inp(0x40) << 8 ;
	mov	DX,TIMER ; l = (TIMER * 0x10000L) + (0xffffL - w) ;
	not	AX

	mov	perform_Rate,11932	; 1193.18K

	push	AX	; push l
	push	DX

	mov	DX,21h	; MASK
	in	AL,DX
	or	AL,1
	out	DX,AL

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	43h,AL
	mov	AL,0ffh
	out	40h,AL
	jmp	$+2
	out	40h,AL

	STI

	push	DS
	lds	DX,LASTTINT
	mov	AX,2508h	; 割り込みベクタ08hの書き込み
	int	21h
	pop	DS

	CLI
	mov	DX,21h
	in	AL,DX
	and	AL,0FEh		; Clear Mask
	out	DX,AL
	STI

	pop	DX	; pop l
	pop	AX

	ret
endfunc			; }

END
