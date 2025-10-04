; 区間の消費時間を測定する[アセンブラ部分]
;
; Function/Procedures:
;	void perform_start(void) ;
;	unsigned long perform_stop(void) ;
;
; Usage:
;   perform_start()を実行した瞬間から、perform_stop()を実行するまでの
;   時間を測定し、perform_stop()の戻り値として返す。
;   　ただし、この値は内部的なものなので、perform_str()を使って実数文字列に
;   変換して利用すること。
; 
; 対象機種:
; 	NEC PC-9801series
; 
; 計測精度:
; 	計測単位: 1/2457.6(1/1996.8)msec
; 	最長範囲: 174760*2(215089.2308*2)msec
; 		→秒換算で約349(430)秒
; 					※値は10MHz系(括弧内は8MHz系)
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
;

	.MODEL SMALL

	.DATA?
	EXTRN perform_Rate:WORD

LASTTINT	dd	?
ACTIVE		dw	?
TIMER		dw	?

	.CODE
	include func.inc

; タイマ割り込みのエントリ
timer_entry	PROC	FAR
	push	 AX
	push	 DS
	mov	 AX,seg TIMER
	mov	 DS,AX
	inc	 TIMER
	pop	 DS
	mov	 AL,20h
	out	 0,AL	
	mov	 AL,30h
	out	 77h,AL		; COUNTER#0, LSB,MSB, Mode0, BinaryCount
	mov	 AL,0FFh
	out	 71h,AL		; counter LSB
	out	 71h,AL		; counter MSB, counter start
	pop	 AX
	iret
	EVEN
timer_entry	ENDP

;
; void perform_start(void) ;
;
func PERFORM_START
	xor	AX,AX
	mov	TIMER,AX
	CLI
	mov	DX,2
	in	AL,DX
	mov	ACTIVE,AX
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
	out	77h,AL

	mov	AL,0FFh
	out	71h,AL		; counter LSB
	out	71h,AL		; counter MSB, counter start

	mov	DX,2
	in	AL,DX
	and	AL,0FEh		; Clear Mask
	out	DX,AL

	STI
	ret
	EVEN
endfunc


;
; unsigned long _far perform_stop(void) ;
; function perform_stop : Longint ;
FUNC PERFORM_STOP
	CLI
	mov	AL,0	; latch
	out	077h,AL

	mov	DX,71h
	in	AL,DX
	mov	AH,AL
	in	AL,DX
	xchg	AH,AL	; w = inp(0x71) + inp(0x71) << 8 ;
	mov	DX,TIMER ; l = (TIMER * 0x10000L) + (0xffffL - w) ;
	not	AX

	push	AX	; push l
	push	DX

	mov	DX,2	; MASK
	in	AL,DX
	or	AL,1
	out	DX,AL

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	77h,AL
	in	AL,42h
	and	AL,20h
	mov	AX,19968	; 周期を10msにする
	jnz	short SKIP1
	mov	AX,24576
SKIP1:
	mov	perform_Rate,AX
	out	71h,AL
	jmp	$+2
	mov	AL,AH
	out	71h,AL

	STI

	push	DS
	lds	DX,LASTTINT
	mov	AX,2508h	; 割り込みベクタ08hの書き込み
	int	21h
	pop	DS

	test	ACTIVE,1
	jne	short SKIP2
		CLI
		mov	DX,2
		in	AL,DX
		and	AL,0FEh		; Clear Mask
		out	DX,AL
		STI
SKIP2:

	pop	DX	; pop l
	pop	AX

	ret
endfunc

END
