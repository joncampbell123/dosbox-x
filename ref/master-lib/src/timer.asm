; master library - PC-9801
;
; Description:
;	タイマ割り込み
;		開始 - timer_start
;		終了 - timer_end
;
; Function/Procedures:
;	int timer_start( unsigned count, void (far * pfunc)(void) ) ;
;	void timer_end(void) ;
;	void timer_leave(void) ;
;
; Parameters:
;	count	タイマ割り込みの周期。単位はタイマによって異なる。
;		0は禁止。
;	pfunc	タイマ割り込み処理ルーチンのアドレス。
;		timer_Countをカウントするだけで他に処理が不要なのなら
;		0を入れてもよい。
;
; Returns:
;	timer_start:
;		0=失敗(すでに動いている)
;		1=成功
;
; Global Variables:
;	void (far * timer_Proc)(void) ;
;	unsigned long timer_Count ;
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
;	　タイマ割り込みルーチンの中では、レジスタの保存条件、仮定などは
;	通常の関数とほぼ同様です。スタックセグメントが不明なことと、
;	割り込みが禁止状態であることに注意してください。
;	　EOIを発行するは必要ありません。
;
;	　timer_startを実行したら、DOSに戻るまでに必ずtimer_endを実行して
;	下さい。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	94/ 3/19 Initial: timer.asm/master.lib 0.23
;	94/ 4/10 [M0.23] bugfix, timer_startの引数を違うところから取ってきてた
;	94/ 6/21 [M0.23] timer_startでtimer_Countを0クリアするようにした

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

TIMER_VECT	EQU 8	; タイマ割り込みのベクタ
IMR		EQU 2	; 割り込みマスクレジスタ
TIMER_DISABLE	EQU 1	; タイマ割り込みのマスクビット


	.DATA
	EXTRN timer_Proc : DWORD	; tim.asm
	EXTRN timer_Count : DWORD	; tim.asm

timer_OldMask DB	0
	EVEN

	.DATA?
timer_OldVect DD	?

	.CODE

TIMER_CNT	equ	71h
TIMER_SET	equ	77h


; TIMER割り込みの初期設定と開始
func TIMER_START	; timer_start() {
	push	BP
	mov	BP,SP
	; 引数
	count	= (RETSIZE+3)*2
	pfunc	= (RETSIZE+1)*2

	xor	AX,AX
	mov	word ptr timer_Count,AX
	mov	word ptr timer_Count+2,AX

	cmp	timer_OldMask,AL ; house keeping
	jne	short S_IGNORE

	mov	AX,[BP+pfunc]
	mov	word ptr timer_Proc,AX
	mov	AX,[BP+pfunc+2]
	mov	word ptr timer_Proc+2,AX

	mov	AL,TIMER_VECT	; TIMER割り込みベクタの設定と保存
	push	AX
	push	CS
	mov	AX,offset TIMER_ENTRY
	push	AX
	call	DOS_SETVECT
	mov	word ptr timer_OldVect,AX
	mov	word ptr timer_OldVect+2,DX

	pushf
	CLI

	mov	AL,36h		; モード(timer#0 MODE#3 LSB+HSB BINARY)
	out	TIMER_SET,AL
	mov	AX,[BP+count]	; 周期
	out	TIMER_CNT,AL
	mov	AL,AH
	out	TIMER_CNT,AL

				; 以前のTIMER割り込みマスクの取得と
	in	AL,IMR		; TIMER割り込みの許可
	mov	AH,AL
	and	AL,NOT TIMER_DISABLE
	out	IMR,AL

	popf

	or	AH,NOT TIMER_DISABLE
	mov	timer_OldMask,AH

	mov	AX,1	; success

S_IGNORE:
	pop	BP
	ret	6
	EVEN
endfunc			; }

; INT 08h TIMER割り込み
TIMER_ENTRY proc far
	push	AX
	push	DS
	mov	AX,seg DGROUP
	mov	DS,AX
	add	word ptr timer_Count,1
	adc	word ptr timer_Count+2,0

	cmp	WORD PTR timer_Proc+2,0
	je	short TIMER_COUNT_END
	push	BX
	push	CX
	push	DX
	push	SI	; for pascal
	push	DI	; for pascal
	push	ES
	CLD
	call	DWORD PTR timer_Proc
	pop	ES
	pop	DI	; for pascal
	pop	SI	; for pascal
	pop	DX
	pop	CX
	pop	BX
	CLI

TIMER_COUNT_END:
	pop	DS
	mov	AL,20h		; EOI
	out	0,AL		; send EOI to master PIC
	pop	AX
	iret
	EVEN
TIMER_ENTRY endp


; TIMER割り込みの終了と復元
	public TIMER_LEAVE
func TIMER_END		; timer_end() {
TIMER_LEAVE label callmodel
	cmp	timer_OldMask,0 ; house keeping
	je	short E_IGNORE

	pushf
	CLI

	xor	BX,BX
	mov	ES,BX
	test	byte ptr ES:[0501H],80h

	mov	AL,36h		; timer#0 MODE#3 LSB+HSB BINARY
	out	TIMER_SET,AL
	mov	AX,19968	; 周期を10msにする
	jnz	short SKIP1
	mov	AX,24576
SKIP1:
	out	TIMER_CNT,AL
	jmp	$+2
	mov	AL,AH
	out	TIMER_CNT,AL

	mov	AX,TIMER_VECT	; TIMER割り込みベクタの復元
	push	AX
	push	word ptr timer_OldVect + 2
	push	word ptr timer_OldVect
	call	DOS_SETVECT

	in	AL,IMR		; TIMER割り込みマスクの復元
	and	AL,timer_OldMask
	out	IMR,AL

	popf

	xor	AL,AL
	mov	timer_OldMask,AL

E_IGNORE:
	ret
endfunc			; }

END
