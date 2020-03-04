; master library - PC-9801
;
; Description:
;	ジョイスティック(キーボード)リアルタイム入力(js_sense)
;	キーボード入力の補強(js_sense2)
;
; Function/Procedures:
;	int js_sense( void ) ;
;	int js_sense2( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	0 - 通常
;	1 = ESCキーが押されている
;	どちらにしても、 js_stat[0]=key/1P, js_stat[1]=2Pの情報が設定されます。
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
;	先に js_start()によってジョイスティックを使用するように初期設定されて
;	いないと、ジョイスティックを無視します。
;
;	js_sense2()を、js_sense()のあと 1ms～数十ms程度遅らせて実行すると、
;	このときにはじめてキーボードの正確な押下状態がjs_statに格納されます。
;	(この遅延は、一般的には vsync_wait()を利用することが多いと思います)
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
;	93/ 5/ 2 Initial:jssense.asm/master.lib 0.16
;	93/ 5/10 SAJ-98に対応したのかなあ?
;	94/ 2/28 [M0.23] js_2player, js_shift, js_keyassign対応…
;	94/ 7/ 3 [M0.23] ESCキー状態を1P4の上(1P5?)に乗せた

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN js_bexist:WORD
	EXTRN js_stat:WORD
	EXTRN js_saj_port:WORD
	EXTRN js_2player:WORD
	EXTRN js_shift:WORD

	.CODE
	EXTRN SOUND_I:NEAR
	EXTRN SOUND_O:NEAR

SENSE_KEY proc near
	xor	BX,BX
	mov	ES,BX
	mov	DI,BX		; joy stick 2(dummy)

	mov	CL,ES:[052eh]	; CL = *    +     L    K    J  H  G   F
	mov	CH,ES:[0531h]	; CH = HELP HMCLR ↓   →   ← ↑ DEL INS
	mov	DX,ES:[0532h]	; DL = tenkey 54  *    9    8  7  /   -
				; DH = tenkey ,0  =    3    2  1  +   6

	; IRST2
	public JS_1P4
JS_1P4:
	mov	AL,ES:[052ch]	; [Q] bit 0
	and	AL,1
	neg	AL
	rcl	BX,1		; **IRST2** = bit 7

	; IRST1
	public JS_1P3
JS_1P3:
	mov	AL,ES:[0538h]	; [SHIFT] bit 0
	and	AL,1
	neg	AL
	rcl	BX,1		; **IRST1** = bit 6

	; トリガ2
	public JS_1P2
JS_1P2:
	mov	AL,ES:[052fh]	; [X] bit 2
	and	AL,4
	mov	AH,ES:[052dh]	; [RETURN] bit 4
	and	AH,10h
	neg	AX
	rcl	BX,1		; **TRIGER2** = bit 5

	; トリガ1
	public JS_1P1
JS_1P1:
	mov	AL,ES:[052fh]	; [Z] bit 1
	and	AL,2
	mov	AH,ES:[0530h]	; [SPACE] bit 4
	and	AH,10h
	neg	AX
	rcl	BX,1		; **TRIGER1** = bit 4

	; 右
	mov	AX,CX		; [→] bit H4, [L] bit L5
	and	AX,1020h
	shr	AL,1
	or	AX,DX		; ten key [9] bit L4
	and	AX,1110h	; ten key [3] bit H4, [6] bit H0
	neg	AX
	rcl	BX,1		; **RIGHT** = bit 3

	; 左
	mov	AX,CX		; [←] bit H3, [H] bit L2
	and	AL,04h
	shr	AH,1
	or	AX,DX		; ten key [1] bit H2
	and	AX,0444h	; ten key [4] bit L6, [7] bit L2
	neg	AX
	rcl	BX,1		; **LEFT** = bit 2

	; 下
	mov	AX,CX		; [↓] bit H5, [J] bit L3
	and	AX,2008h
	or	AL,DH
	and	AL,1ch		; ten key [1][2][3] bit 2,3,4
	neg	AX
	rcl	BX,1		; **DOWN** = bit 1

	; 上
	mov	AX,CX		; [↑] bit H2, [K] bit L4
	and	AX,0410h
	or	AL,DL
	and	AL,1ch		; ten key [7][8][9] bit 2,3,4
	neg	AX
	rcl	BX,1		; **UP** = bit 0

	mov	SI,BX		; joy stick 1	(***orではなくmovしてるぞ***)

	cmp	js_2player,0
	je	short NO_2P

	; 2player トリガ2
	public JS_2P2
JS_2P2:
	mov	AL,ES:[052ah+3]	; [D] bit 7
	and	AL,80h
	neg	AL
	rcl	DI,1		; **TRIGER2** = bit 5

	; 2player トリガ1
	public JS_2P1
JS_2P1:
	mov	AL,ES:[052ah+0eh] ; [GRPH] bit 3
	and	AL,8
	neg	AL
	rcl	DI,1		; **TRIGER1** = bit 4

	; 右
	public JS_2PRIGHT
JS_2PRIGHT:
	mov	AL,ES:[052ah+4]	; [F] bit 0
	and	AL,1
	neg	AL
	rcl	DI,1		; **RIGHT** = bit 3

	; 左
	public JS_2PLEFT
JS_2PLEFT:
	mov	AL,ES:[052ah+3]	; [S] bit 6
	and	AL,40h
	neg	AL
	rcl	DI,1		; **LEFT** = bit 2

	; 下
	public JS_2PDOWN
JS_2PDOWN:
	mov	AL,ES:[052ah+5]	; [C] bit 3
	and	AL,8
	neg	AL
	rcl	DI,1		; **DOWN** = bit 1

	; 上
	public JS_2PUP
JS_2PUP:
	mov	AL,ES:[052ah+2]	; [E] bit 2
	and	AL,4
	neg	AL
	rcl	DI,1		; **UP** = bit 0

NO_2P:
	mov	AL,ES:[052ah]
	and	AX,1		; [ESC]

	mov	BL,0		; merge ESC
	mov	BH,AL
	or	SI,BX
	ret
SENSE_KEY endp




SOUND_JOY proc near
	mov	BH,0fh
	call	SOUND_O

	mov	DX,188h
	mov	AL,0eh
	out	DX,AL

	inc	DX
	inc	DX
	in	AL,DX
	not	AL
	ret
SOUND_JOY endp

func JS_SENSE	; {
	push	BP
	push	SI	; joy stick 1
	push	DI	; joy stick 2
	xor	BP,BP

	call	SENSE_KEY
	mov	CX,AX

	cmp	js_bexist,0
	je	short NO_STICK
	pushf
	CLI
	; ジョイスティックからの読み取り

	mov	BL,080h		; JOYSTICK1
	call	SOUND_JOY
	and	AX,003fh	; b,a,right,left,forward,back
	mov	BP,AX

	mov	BL,0c0h		; JOYSTICK2
	call	SOUND_JOY
	and	AX,003fh
	or	DI,AX
	popf
NO_STICK:
	mov	DX,js_saj_port
	test	DX,DX
	je	short NO_SAJ
	push	CX
	mov	CX,2
	mov	BX,DX
SAJ_IN:
	mov	AX,30h	; digital read
	call	SAJOUT
	and	AX,3fh	; b,a,right,left,forward,back
	or	BP,AX
	inc	DX
	inc	DX
	xchg	SI,DI
	loop	short SAJ_IN
	pop	CX
NO_SAJ:
	mov	AX,CX
	cmp	js_shift,0
	je	short NO_SHIFT
	or	DI,BP
	xor	BP,BP
NO_SHIFT:
	or	SI,BP
	mov	js_stat,SI
	mov	js_stat + 2,DI

	pop	DI
	pop	SI
	pop	BP
	ret
endfunc		; }


SAJOUT	proc near
	xchg	DX,BX
	out	DX,AL
	xchg	DX,BX
	in	AL,DX
	not	AL
	ret
SAJOUT	endp



func JS_SENSE2	; {
	push	SI
	push	DI

	call	SENSE_KEY

	or	js_stat,SI
	or	js_stat + 2,DI
	pop	DI
	pop	SI
	ret
endfunc		; }


END
