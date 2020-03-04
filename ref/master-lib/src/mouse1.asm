; Description:
;	マウスドライバ(第１層)
;		ハードウェアアクセス
;
; Functions/Procedures:
;	void mouse_int_start( void (far * mousefunc)(), int freq ) ;
;	void mouse_int_end( void ) ;
;	void mouse_int_enable( void ) ;
;	void mouse_int_disable( void ) ;
;
; Parameters:
;	mousefunc	割り込み関数・通常は mouse_procを使う
;			segment = 0で渡すと、割り込みを禁止する。
;	int freq	割り込み周期 0 = 120Hz, 1 = 60Hz, 2 = 30Hz, 3 = 15Hz
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
;	92/11/24 Initial
;	93/ 1/31 割り込みなし bugfix
;	93/ 7/14 [M0.20] FARCODE: 起動時にマウス割り込みが生きていたら
;	                 復元するときに死亡(^^;
;	93/ 7/20 [M0.20] int15の中でEOI発行するときに割り込み禁止して
;	                 なかったぞ(^^;修正だ

	.MODEL SMALL
	include func.inc
	EXTRN DOS_SETVECT:CALLMODEL

INTMASK = 0dfh

	.DATA?

old_int15	dd	?	; int 15h ( mouse )
old_mask	dw	?
mouse_IntBody	dd	?

	.CODE

func MOUSE_INT_START	; mouse_int_start() {
	mov	BX,SP
	; 引数
	mousefunc = (RETSIZE+1)*2
	freq	  = (RETSIZE)*2

	les	DX,SS:[BX+mousefunc]
	mov	BX,SS:[BX+freq]

	mov	word ptr mouse_IntBody,DX
	mov	word ptr mouse_IntBody+2,ES

	CLI
	xor	AX,AX
	mov	DX,7fdfh
	in	AL,0ah
	mov	old_mask,AX
	call	CALLMODEL PTR DISABLE_FORCE
	STI

	mov	DX,7fdfh	; mouse mode set = 93
	mov	AL,93h		;
	out	DX,AL		;

	mov	DX,0bfdbh	; 割り込み周波数の設定
	mov	AL,BL
	out	DX,AL

	mov	AX,15h
	push	AX
	push	CS
	mov	AX,offset int15
	push	AX
	call	DOS_SETVECT
	mov	WORD PTR old_int15,AX
	mov	WORD PTR old_int15+2,DX

	cmp	WORD PTR mouse_IntBody + 2, 0
	jz	short SKIP_ENABLE

	call	CALLMODEL PTR MOUSE_INT_ENABLE

SKIP_ENABLE :
	ret 6
endfunc			; }


func MOUSE_INT_END	; mouse_int_end() {
	pushf
	CLI
	call	CALLMODEL PTR MOUSE_INT_DISABLE

	mov	DX,0bfdbh	; 割り込み周波数の設定
	mov	AL,0		; 120Hz
	out	DX,AL

	push	DS
	mov	AX,2515h
	lds	DX,old_int15
	int	21h
	pop	DS

	mov	AL,NOT INTMASK
	test	BYTE PTR old_mask,AL
	jne	short DISABLED
		call	CALLMODEL PTR ENABLE_FORCE
DISABLED:
	mov	DX,7fdfh
	mov	AL,93h
	out	DX,AL
	popf
END_IGNORE:
	ret
ENDFUNC			; }


func MOUSE_INT_DISABLE	; mouse_int_disable() {
	cmp	WORD PTR mouse_IntBody+2,0
	je	short DISABLE_IGNORE
DISABLE_FORCE label CALLMODEL
	push	AX
	push	DX
	mov	AL,9
	mov	DX,7fdfh	; マウス割り込み Disable
	out	DX,AL
	in	AL,0ah
	or	AL,not INTMASK	; Slave PICのマウス割り込み禁止
	out	0ah,AL
	pop	DX
	pop	AX
DISABLE_IGNORE:
	ret
endfunc			; }


func MOUSE_INT_ENABLE	; mouse_int_enable() {
	cmp	WORD PTR mouse_IntBody+2,0
	je	short ENABLE_IGNORE
ENABLE_FORCE label CALLMODEL
	CLI
	push	AX
	push	DX
	mov	AL,8
	mov	DX,7fdfh
	out	DX,AL			; マウス割り込み Enable
	in	AL,0ah			; Slave PICのマウス割り込み許可
	and	AL,INTMASK
	out	0ah,AL
	pop	DX
	pop	AX
	STI
ENABLE_IGNORE:
	ret
endfunc			; }

; ハードウェア割り込みルーチン
int15	proc	far
	push	AX
	push	BX
	push	CX
	push	DX
	push	DS
	push	ES

	CLD
	mov	AX,seg DGROUP
	mov	DS,AX
	mov	ES,AX

	call	MOUSE_INT_DISABLE
	STI

	call  	DWORD PTR mouse_IntBody	; SI,DIは保護せよ

	CLI

	; 割り込み終了
	mov	DX,8
	mov	AL,20h			; send EOI to Slave PIC
	out	DX,AL
	mov	AL,0bh			; no poll, ISR read
	out	DX,AL			;
	in	AL,DX			; read Slave ISR
	test	AL,AL			; slave割り込み処理中があるか
	jnz	short HOEHOE
		mov	AL,20h		; ないならば
		out	0,AL		; send EOI to Master PIC
HOEHOE:
	call	MOUSE_INT_ENABLE

	pop	ES
	pop	DS
	pop	DX
	pop	CX
	pop	BX
	pop	AX
	iret
int15	endp

END
