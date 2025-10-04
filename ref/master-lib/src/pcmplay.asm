; master library - PC98V - PCM
;
; Description:
;	BEEP音源による 8bitPCM再生
;
; Function:
;	void pcm_play(const void far *pcm,unsigned rate,unsigned long size);
;
; Parameters:
;	const void far * pcm	= PCMデータの先頭アドレス
;	unsigned rate		= 演奏レート(タイマ周波数/再生周波数)(1~255)
;				  22KHzが適性値です。
;	unsigned long size	= パルス数(0なら演奏しにゃい)
;
; Returns:
;	none
;
; Binding Target:
;	MSC/TC/BC/TP
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: 8086
;
; Compiler/Assembler:
;	OPTASM 1.6
;	TASM 2.0
;
; Notes:
;	pcmデータは、pcm_convertの変換結果でなければなりません。
;	22KHzならば通常の音が鳴るという程度の出来です。
;
;	演奏中は割り込みを禁止しています。(長時間なので注意)
;	演奏後、BEEP周波数は既定値(2KHz)に、
;	タイマ割り込みの周期も既定値(10ms)に設定します。
;
; Author:
;	新井俊一 (SuCa: pcs28991 fem20932)
;	恋塚昭彦 (恋塚: net19368 fem20047)
;
; Rivision History:
;	92/10/13 Initial
;	92/10/18 Beep復旧機能のBugfix
;		 ついでにデータを壊さなくしたにょろ。
;	92/10/24 16bitRateに変更(test)。
;		 計算を分離
;	92/10/26 Rate自動判別機能搭載
;	92/12/09 恋塚: 8bit固定, TIMER使用
;	92/12/10 いきなり微調整(ノイズが気になった)
;	93/ 1/13 割り込み禁止をタイマ(とvsync)の割り込み禁止だけにした
;	93/ 2/ 4 バグ取り(割り込み復元が辺(もとい編)だった)
;	95/ 1/16 [M0.23] CLI/STIをpushf+CLI/popfに変更。

	.MODEL SMALL
	include func.inc
	.DATA?
ACTIVE db ?

	.CODE

IMR equ 2
TIMM equ 1
VSYM equ 4

func PCM_PLAY
	push	BP
	mov	BP,SP

	; 引数
	pcm_seg	= (RETSIZE + 5)*2
	pcm_off	= (RETSIZE + 4)*2
	rate	= (RETSIZE + 3)*2
	pcm_lh	= (RETSIZE + 2)*2
	pcm_ll	= (RETSIZE + 1)*2

	push	DS
	push	SI
	push	DI

	; タイマ・VSYNC割り込み禁止
	pushf
	CLI
	mov	DX,IMR
	in	AL,DX
	mov	AH,AL
	or	AH,NOT (TIMM OR VSYM)
	mov	ACTIVE,AH
	or	AL,TIMM or VSYM		; MASK!
	out	DX,AL
	mov	AL,0
	popf

	mov	AL,07h			; BEEP OFF
	out	37h,AL

	mov	AL,14h			; timer#0 MODE#2 LSB BINARY
	out	77h,AL

	mov	AL,50h			; timer#1 MODE#0 LSB BINARY
	mov	DX,3FDFh
	out	DX,AL

	lds	SI,[BP+pcm_off]		; DS:SI = データアドレス
	mov	BX,DS
	mov	CX,[BP+pcm_ll]		; DICX = データ長
	mov	DI,[BP+pcm_lh]
	mov	AL,[BP+rate]
	out	71h,AL			; timer#0 演奏レート指定

	jmp	short PLAYS
	EVEN
WAITHI:
	in	AL,71h
	cmp	AL,16
	jb	short WAITHI	; aroundを待つ

WAITREQ:
;	jmp	$+2
	in	AL,71h
	cmp	AL,16
	jnb	short WAITREQ	; aroundを待つ
;	mov	AL,07h		; BEEP OFF
;	out	37h,AL

	mov	AL,AH
	mov	DX,3FDBh	; 指定時間後に BEEP ON
	out	DX,AL

	mov	AL,06h		; BEEP ON
	out	37h,AL
	cmp	SI,1
	sbb	DX,DX
	and	DX,1000h
	add	BX,DX
	mov	DS,BX
PLAYS:
	lodsb			; 1バイト取る
	mov	AH,AL
	sub	CX,1
	sbb	DI,0
	jnb	short WAITHI

PLAY8E:				; counter hi-word decrement

	mov	AL,07h		; BEEP OFF
	out	37h,AL
	;----------
	; 演奏終了
	;----------

	; timer#0は 10msの既定値に設定

	mov	AL,76h		; timer復元
	mov	DX,3FDFh
	out	DX,AL
	in	AL,42h
	and	AL,20h
	mov	AX,998
	mov	BX,19968
	jnz	short SKIP	; BEEP音の周波数を通常に戻す
	mov	AX,1229
	mov	BX,24576
EVEN
SKIP:
	mov	DX,3FDBh
	out	DX,AL
	mov	AL,AH
	out	DX,AL

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	77h,AL
	jmp	$+2
	jmp	$+2
	mov	AX,BX
	out	71h,AL
	jmp	$+2
	mov	AL,AH
	out	71h,AL

	pop	DI
	pop	SI
	pop	DS

	; タイマ・VSYNC割り込みを許可
	mov	AH,ACTIVE
	cmp	AH,0ffh
	je	short SKIP2
		pushf
		CLI
		mov	DX,2
		in	AL,DX
		and	AL,AH
		out	DX,AL
		out	64h,AL	; kick vsync
		popf
SKIP2:

	pop	BP
	ret	10
endfunc

END
