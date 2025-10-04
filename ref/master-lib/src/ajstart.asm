FASTSTICK equ 1
; master library - PC/AT JoyStick RTC
;
; Description:
;	PC/AT keyboard, JoyStickのセンス
;
; Function/Procedures:
;	int at_js_start( int mode ) ;
;	void at_js_end(void) ;
;	int at_js_sense(void) ;
;	void at_js_calibrate(Point* min, Point* max, Point* center);
;	int at_js_wait(Point *p) ;
;
; Parameters:
;	mode 0 (JS_NORMAL)    = Joy Stick の存在を検査する
;       1 (JS_FORCE_USE) = Joy Stick の存在を検査しない
;       2 (JS_IGNORE)    = Joy Stick の存在を検査しない
;
; Returns:
;	0 = ジョイスティックは全く使用しない
;	1 = ジョイスティックを認識した
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	js_bexist = 0 不在,認識しない
;	js_bexist = 1 存在,キャリブレートしてない
;	js_bexist = 2 存在,キャリブレートした
;
;	at_js_sense は、キャリブレートしないとjoystick位置は読み取りません。
;	ただし、ボタンは読み取ります。
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
;	93/12/19 Initial: ajstart.asm/master.lib 0.22
;	94/ 5/17 [M0.23] ジョイスティック対応
;	94/ 5/25 [M0.23] joy stickがつながってるとESCが利かなくなってた
;	94/ 6/22 [M0.23] 速度検査タイマを0から2(beep freq)に変更
;	94/ 7/ 3 [M0.23] ESCキー状態を1P4の上(1P5?)に乗せた
;	94/ 7/ 3 [M0.23] at_js_fast=1にしてから開始するとデジタル専用モードに
;	94/12/29 [M0.23] tenkeyのEnterに対応(^^;
;	95/ 1/ 7 [M0.23] 常駐ブロック対応
;	95/ 1/28 [M0.23] Home/Hend/PgUp/PgDnでのナナメ移動廃止
;	95/ 2/19 [M0.22k] 2Pのstickを無視
;	95/ 2/23 [M0.22k] RTC割り込みマネージャrtc_int_set使用

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc		; 計測にbeep使うので復元用

	EXTRN DOS_SETVECT:CALLMODEL
	EXTRN DOS_KEYCLEAR:CALLMODEL

GAME_PORT	equ 201h
TIMER_COUNT2	equ 042h
TIMER_CMD	equ 043h

DUMMYTIME	equ 2000	; sampling count
TIMEOUT		equ 2000	; about 2ms

UP_BIT		equ 1
DOWN_BIT	equ 2
LEFT_BIT	equ 4
RIGHT_BIT	equ 8

JFAST_MAX	equ 12		; 12/4096 sec

JS_TRIG1 	equ 00010000b
JS_TRIG2 	equ 00100000b

	.DATA
	EXTRN js_bexist:WORD
	EXTRN js_stat:WORD
	EXTRN js_2player:WORD
	EXTRN js_shift:WORD

	; variables; defined in aj.asm
	EXTRN at_js_resseg:WORD
	EXTRN at_js_count:WORD
	EXTRN at_js_x1:WORD
	EXTRN at_js_y1:WORD
	EXTRN at_js_x2:WORD
	EXTRN at_js_y2:WORD
	EXTRN at_js_mintime:WORD
	EXTRN at_js_maxtime:WORD
	EXTRN at_js_min:WORD
	EXTRN at_js_max:WORD

at_js_count2	dw	0
if FASTSTICK
	EXTRN at_js_fast:WORD
jsdiv	dw	0
jsx1	dw	0
jsy1	dw	0
jsx2	dw	0
jsy2	dw	0

endif

	.DATA?
at_js_lx	dw	?	; 左、上へのしきい値
at_js_rx	dw	?	; 右、下へのしきい値
at_js_uy	dw	?	; 左、上へのしきい値
at_js_ly	dw	?	; 右、下へのしきい値

	.CODE

	public JS_MAP
NUMMAP	equ 16*2
JS_MAP	db	NUMMAP dup (0)
e0flag	dw	0

org_key_int dd 0

if FASTSTICK
	EXTRN rtc_int_set:NEAR		; in ATRTCMOD.ASM
RTC_SLOT_JOYSTICK	equ 1		; joystick RTC割り込みスロット番号
endif

	even

key_int	proc far
	push	AX
	push	BX
	push	CX
	in	AL,60h		; key data port
	cmp	AL,0e0h
	je	short e0set

	mov	BX,AX
	and	BX,07fh
	mov	CL,AL
	and	CL,7
	shr	BX,3
	cmp	CS:e0flag,0
	je	short NOEXTEND
	add	BL,16
	mov	CS:e0flag,0
NOEXTEND:

	test	AL,80h
	jz	short make_code
break_code:
	mov	AL,not 1
	rol	AL,CL
	and	CS:JS_MAP[BX],AL
	jmp	short go_original
	EVEN
e0set:
	mov	CS:e0flag,1
	jmp	short go_original
	EVEN

make_code:
	mov	AL,1
	rol	AL,CL
	or	CS:JS_MAP[BX],AL
go_original:
;	mov	AL,0feh	; 再送要求
;	out	64h,AL
	pop	CX
	pop	BX

	pop	AX
	jmp	dword ptr CS:org_key_int
	EVEN
key_int	endp

if FASTSTICK
rtc_int proc near
	push	DX
	cmp	at_js_fast,0
	je	short RTCINT_SKIP
	mov	DX,GAME_PORT

	dec	jsdiv
	jg	short ATJS_SENSE

	mov	jsdiv,JFAST_MAX

	out	DX,AL		; trigger one-shot
	xor	AX,AX
	xchg	AX,jsx1
	mov	at_js_x1,AX
	xor	AX,AX
	xchg	AX,jsy1
	mov	at_js_y1,AX
	xor	AX,AX
	xchg	AX,jsx2
	mov	at_js_x2,AX
	xor	AX,AX
	xchg	AX,jsy2
	mov	at_js_y2,AX

ATJS_SENSE:
	in	AL,DX
	shr	AL,1
	adc	jsx1,0
	shr	AL,1
	adc	jsy1,0
	shr	AL,1
	adc	jsx2,0
	shr	AL,1
	adc	jsy2,0

RTCINT_SKIP:
	pop	DX
	ret
rtc_int	endp
endif


STICKMASK equ 03h

; in: CX = maxcount
; out: AL = buttons( B2 A2 B1 A1 )
;      BX = x1
;      SI = y1
;      DI = x2
;      BP = y2
joy_read proc near
	mov	DX,GAME_PORT
	out	DX,AL		; trigger one-shot

	xor	BX,BX		; BX = x1
	mov	SI,BX		; SI = y1
	mov	DI,BX		; DI = x2
	mov	BP,BX		; BP = y2
JOY_1:
	in	AL,DX
	test	AL,STICKMASK
	jz	short JOY_1

JOY_2:
	in	AL,DX
	shr	AL,1
	adc	BX,0
	shr	AL,1
	adc	SI,0
	shr	AL,1
	adc	DI,0
	shr	AL,1
	adc	BP,0
	loop	short JOY_2
	ret
	EVEN
joy_read endp

; ジョイスティック用準備
joy_init proc near
	push	BP
	push	SI
	push	DI

	pushf
	CLI

	in	AL,61h
	and	AL,00001100b	; BEEP OFF
	or	AL,00000001b
	out	61h,AL

	mov	CX,DUMMYTIME	;

	mov	AL,0b0h		; COUNTER#2, LSB,MSB, Mode0, BinaryCount
	out	TIMER_CMD,AL
	mov	AL,0FFh
	out	TIMER_COUNT2,AL	; counter LSB
	out	TIMER_COUNT2,AL	; counter MSB, counter set
	call	joy_read
	mov	AL,80h		; latch counter#2
	out	TIMER_CMD,AL
	in	AL,TIMER_COUNT2
	mov	CL,AL
	in	AL,TIMER_COUNT2
	mov	CH,AL

	; BGMがおかしくなるのを直す
	mov	AL,0b6h	; CNT#2, L-H WORD, 方形波, binary
	out	TIMER_CMD,AL		; AT
	mov	AX,TVALATORG/2		; bgm.inc
	out	TIMER_COUNT2,AL		; AT
	mov	AL,AH
	out	TIMER_COUNT2,AL		; AT

	popf

	not	CX
	mov	AX,DUMMYTIME
	mov	DX,TIMEOUT
	mul	DX
	div	CX
	mov	at_js_count,AX		; TIMEOUT時間に達するのに必要な回数
	mov	at_js_count2,AX
	mov	BX,AX

	mov	AX,at_js_mintime
	mul	BX
	mov	CX,TIMEOUT
	div	CX
	mov	at_js_min,AX		; 最小値のカウント

	mov	AX,at_js_maxtime
	mul	BX
	mov	CX,TIMEOUT
	div	CX
	mov	at_js_max,AX		; 最大値のカウント

	sub	AX,at_js_min
	shr	AX,1			; 片側の範囲
	mov	BX,AX
	add	AX,at_js_min		; 中央値
	shr	BX,1
	shr	BX,1			; 片側の範囲の1/4

	sub	AX,BX
	mov	at_js_lx,AX		; デジタル処理の小さい方のしきい値
	mov	at_js_uy,AX
	add	AX,BX
	add	AX,BX
	mov	at_js_rx,AX		; デジタル処理の大きい方のしきい値
	mov	at_js_ly,AX
if FASTSTICK
	cmp	at_js_fast,0
	je	short NO_RTCINT_SET

	mov	at_js_count2,JFAST_MAX
	mov	at_js_min,0
	mov	at_js_max,8
	mov	at_js_lx,4
	mov	at_js_rx,4
	mov	at_js_uy,3
	mov	at_js_ly,3

	mov	BX,RTC_SLOT_JOYSTICK
	mov	AX,offset rtc_int
	call	rtc_int_set

NO_RTCINT_SET:
endif

	pop	DI
	pop	SI
	pop	BP
	ret
	EVEN
joy_init endp

func AT_JS_START ; at_js_start() {
	push	BP
	mov	BP,SP
	push	DI
	; 引き数
	mode = (RETSIZE+1)*2

	CLD

	mov	AX,word ptr CS:org_key_int
	or	AX,word ptr CS:org_key_int+2
	jnz	short START_SKIP
	; AX=0

	mov	CX,NUMMAP/2
	push	CS
	pop	ES
	mov	DI,offset JS_MAP
	; xor	AX,AX
	rep	stosw

	push	9
	push	CS
	push	offset key_int
	call	DOS_SETVECT
	mov	word ptr CS:org_key_int,AX
	mov	word ptr CS:org_key_int+2,DX

	xor	AX,AX
	cmp	[BP+mode],AX
	jnz	short IGNORE_STICK
	mov	DX,GAME_PORT
	in	AL,DX
	add	AL,1
	sbb	AX,AX	; not exist=-1  exist=0
	inc	AX	; not exist=0   exist=1
IGNORE_STICK:
	mov	js_bexist,AX
	test	AX,AX
	jz	short START_SKIP
	call	joy_init
START_SKIP:
	call	DOS_KEYCLEAR
	mov	AX,js_bexist
	pop	DI
	pop	BP
	ret	2
endfunc		; }

func AT_JS_END	; at_js_end() {
	mov	AX,word ptr CS:org_key_int
	or	AX,word ptr CS:org_key_int+2
	jz	short END_SKIP

if FASTSTICK
	mov	BX,RTC_SLOT_JOYSTICK
	mov	AX,0
	call	rtc_int_set
NO_RTCINT_BACK:
endif

	push	9
	push	word ptr CS:org_key_int+2
	push	word ptr CS:org_key_int
	call	DOS_SETVECT
	xor	AX,AX
	mov	word ptr CS:org_key_int,AX
	mov	word ptr CS:org_key_int+2,AX
END_SKIP:
	call	DOS_KEYCLEAR
	ret
endfunc		; }

; key code:
;	Left Shift 2ah    Right Shift 36h
;	Ctrl 1dh   Alt 38h   PrintScreen 37h   ScrollLock 46h
;	H 23h   J 24h   K 25h   L 26h   Q 10h
;	Space 39h   Return 1ch   Tab 0fh    Esc 01h
;	ten key:		cursor key:
;	47h 48h 49h		    48h
;	4bh 4ch 4dh		4bh 50h 4dh
;	4fh 50h 51h

SENSE_KEY proc near
	xor	BX,BX
	mov	ES,BX
	mov	DI,BX		; joy stick 2(dummy)

	mov	CL,CS:JS_MAP[4]	   ; CL = M     L    K    J    H  F  D   S
	mov	DX,word ptr CS:JS_MAP[8+16]
	;          1 654 987
	and	DX,0111110101111111b
	or	DX,word ptr CS:JS_MAP[8]
	mov	CH,CS:JS_MAP[10+16]
	;                32
	and	CH,00000001b
	or	CH,CS:JS_MAP[10]

	; 左右shiftのorを 0:0に再マップ
	; AL=[SHIFT](left) bit 2    AH=(right) bit 6
	and	CS:JS_MAP[0],NOT 01h
	mov	AX,word ptr CS:JS_MAP[5]
	and	AX,4004h
	neg	AX
	adc	CS:JS_MAP[0],0	; set bit0

	; IRST2
	public AT_JS_1P4
AT_JS_1P4:
	mov	AL,CS:JS_MAP[2]	; [Q] bit 0
	and	AL,1
	neg	AL
	rcl	BX,1		; **IRST2** = bit 7

	; IRST1
	public AT_JS_1P3
AT_JS_1P3:
	mov	AL,CS:JS_MAP[0]	; [Shift]
	and	AL,1
	neg	AL
	rcl	BX,1		; **IRST1** = bit 6

	; トリガ2
	public AT_JS_1P2
AT_JS_1P2:
	mov	AL,CS:JS_MAP[5]	; [X] bit 5
	and	AL,20h
	mov	AH,CS:JS_MAP[3]	; [Enter] bit 4
	or	AH,CS:JS_MAP[16+3]; tenkey [Enter] bit 4
	and	AH,10h
	neg	AX
	rcl	BX,1		; **TRIGER2** = bit 5

	; トリガ1
	public AT_JS_1P1
AT_JS_1P1:
	mov	AL,CS:JS_MAP[5]	; [Z] bit 4
	and	AL,10h
	mov	AH,CS:JS_MAP[7]	; [SPACE] bit 1
	and	AH,02h
	neg	AX
	rcl	BX,1		; **TRIGER1** = bit 4

	; 右
	mov	AX,CX		; 
	and	AX,0240h
	shr	AL,1
	or	AL,DH		; 
	and	AL,22h		; 
	neg	AX
	rcl	BX,1		; **RIGHT** = bit 3

	; 左
	mov	AH,CL		;
	and	AX,0800h
	or	AX,DX
	and	AX,8880h
	neg	AX
	rcl	BX,1		; **LEFT** = bit 2

	; 下
	mov	AH,DH
	rol	AH,1
	and	AX,100h
	or	AX,CX		;
	and	AX,0310h
	neg	AX
	rcl	BX,1		; **DOWN** = bit 1

	; 上
	mov	AL,CL
	shl	AL,1
	shl	AL,1
	and	AX,80h
	or	AX,DX
	and	AX,0380h
	neg	AX
	rcl	BX,1		; **UP** = bit 0

	mov	AL,CS:JS_MAP[0]
	shr	AL,1		; [ESC]
	and	AX,1

;	or	BH,AL		; merge ESC key

	mov	SI,BX		; joy stick 1	(***orではなくmovしてるぞ***)

	cmp	js_2player,0
	je	short NO_2P

	; 2player トリガ2
	public AT_JS_2P2
AT_JS_2P2:
	mov	AL,CS:JS_MAP[4]	; [D] bit 0
	and	AL,01h
	neg	AL
	rcl	DI,1		; **TRIGER2** = bit 5

	; 2player トリガ1
	public AT_JS_2P1
AT_JS_2P1:
	mov	AL,CS:JS_MAP[7]	; [ALT] bit 0
	and	AL,1
	neg	AL
	rcl	DI,1		; **TRIGER1** = bit 4

	; 右
	public AT_JS_2PRIGHT
AT_JS_2PRIGHT:
	mov	AL,CS:JS_MAP[4]	; [F] bit 1
	and	AL,2
	neg	AL
	rcl	DI,1		; **RIGHT** = bit 3

	; 左
	public AT_JS_2PLEFT
AT_JS_2PLEFT:
	mov	AL,CS:JS_MAP[3]	; [S] bit 7
	and	AL,80h
	neg	AL
	rcl	DI,1		; **LEFT** = bit 2

	; 下
	public AT_JS_2PDOWN
AT_JS_2PDOWN:
	mov	AL,CS:JS_MAP[5]	; [C] bit 6
	and	AL,40h
	neg	AL
	rcl	DI,1		; **DOWN** = bit 1

	; 上
	public AT_JS_2PUP
AT_JS_2PUP:
	mov	AL,CS:JS_MAP[2]	; [E] bit 2
	and	AL,4
	neg	AL
	rcl	DI,1		; **UP** = bit 0

NO_2P:
	mov	AL,CS:JS_MAP[0]
	shr	AL,1		; [ESC]
	and	AX,1

	mov	BL,0		; merge ESC
	mov	BH,AL
	or	SI,BX
	ret
	EVEN
SENSE_KEY endp

func AT_JS_SENSE ; at_js_sense() {
	push	SI
	push	DI

	call	SENSE_KEY

	cmp	js_bexist,1
	jge	short READ_STICK
	mov	js_stat,SI
	mov	js_stat + 2,DI
	pop	DI
	pop	SI
	ret
	EVEN

	; ジョイスティックからの読み取り
READ_STICK:
	push	BP

if FASTSTICK
	cmp	at_js_fast,0
	jne	short FAST
endif
	push	SI
	push	DI
	mov	CX,at_js_count
	call	joy_read
	mov	at_js_x1,BX
	mov	at_js_y1,SI
	mov	at_js_x2,DI
	mov	at_js_y2,BP
	mov	CX,SI
	mov	DX,DI
	pop	DI
	pop	SI

if FASTSTICK
	jmp	short SENSESTICK
FAST:
	mov	BX,at_js_x1
	mov	CX,at_js_y1
	mov	DX,at_js_x2
	mov	BP,at_js_y2
SENSESTICK:
endif
	xchg	AX,BP
	mov	BP,0
	cmp	js_bexist,1
	je	short READ_BUTTON
	xchg	BP,AX

IF 0				; ignore 2P stick ----start
	cmp	DX,BP
	jne	short JGO2
	cmp	DX,at_js_count2
	je	short J2SKIP
JGO2:
	mov	AL,RIGHT_BIT
	cmp	DX,at_js_rx	; x2 right
	ja	short JX2S
	cmp	DX,at_js_lx	; x2 left
	jae	short JY2
	mov	AL,LEFT_BIT
JX2S:	or	DI,AX
JY2:
	mov	AL,DOWN_BIT
	cmp	BP,at_js_ly	; y2 down
	ja	short JY2S
	cmp	BP,at_js_uy	; y2 up
	jae	short JY2E
	mov	AL,UP_BIT
JY2S:	or	DI,AX
JY2E:
ENDIF				; ignore 2P stick ----end
J2SKIP:
	xor	BP,BP
	cmp	BX,CX
	jne	short JGO1
	cmp	BX,at_js_count2
	je	short JSKIP1
JGO1:
	mov	AL,RIGHT_BIT
	cmp	BX,at_js_rx	; x1 right
	jae	short JX1S
	cmp	BX,at_js_lx	; x1 left
	ja	short JX1E
	mov	AL,LEFT_BIT
JX1S:	or	BP,AX
JX1E:	mov	AL,DOWN_BIT
	cmp	CX,at_js_ly	; y1 down
	jae	short JY1S
	cmp	CX,at_js_uy	; y1 up
	ja	short JY1E
	mov	AL,UP_BIT
JY1S:	or	BP,AX
JY1E:
READ_BUTTON:
JSKIP1:
	; Trigger Buttons
	mov	DX,GAME_PORT
	in	AL,DX
	not	AX
	and	AX,0f0h
	mov	BX,AX
	and	BL,030h
	shr	AX,2
	and	AL,030h
	or	BP,BX
	or	DI,AX

	cmp	js_shift,0
	je	short NOSHIFT
	or	DI,BP
	xor	BP,BP
NOSHIFT:
	or	SI,BP

	pop	BP

	mov	js_stat,SI
	mov	js_stat + 2,DI

	mov	AX,SI
	mov	AL,AH		; ESCの状態
	and	AX,1

	pop	DI
	pop	SI
	ret
endfunc		; }

func AT_JS_CALIBRATE ; at_js_calibrate() {
	push	BP
	mov	BP,SP
	push	SI
	cmp	js_bexist,1
	jl	short CAL_IGNORE

	; 引き数
	min    = (RETSIZE+5)*2
	max    = (RETSIZE+3)*2
	center = (RETSIZE+1)*2

	push	DS
	lds	BX,[BP+center]
	mov	SI,[BX]		; center.x
	mov	CX,SI

	lds	BX,[BP+min]
	mov	AX,[BX]		; min.x
	sub	SI,AX		; SI = center.x - min.x

	lds	BX,[BP+max]
	mov	AX,[BX]
	sub	AX,CX		; AX = max.x - center.x

	cmp	AX,SI
	jl	short CAL_SKIPX
	mov	AX,SI		; AX = min(AX,SI)
CAL_SKIPX:
	cwd
	mov	SI,3
	idiv	SI		; AX = AX/3
	cmp	AX,1
	jge	short CAL_SKIPX2
	mov	AX,1		; if AX < 1 then AX = 1
CAL_SKIPX2:
	pop	DS
	mov	SI,CX
	sub	SI,AX	; lx
	add	CX,AX	; rx
	mov	at_js_lx,SI
	mov	at_js_rx,CX


	push	DS
	lds	BX,[BP+center]
	mov	SI,[BX+2]	; center.y
	mov	CX,SI

	lds	BX,[BP+min]
	mov	AX,[BX+2]	; min.y
	sub	SI,AX		; SI = center.y - min.y

	lds	BX,[BP+max]
	mov	AX,[BX+2]
	sub	AX,CX		; AX = max.y - center.y

	cmp	AX,SI
	jl	short CAL_SKIPY
	mov	AX,SI		; AX = min(AX,SI)
CAL_SKIPY:
	cwd
	mov	SI,3
	idiv	SI		; AX = AX/3
	cmp	AX,1
	jge	short CAL_SKIPY2
	mov	AX,1		; if AX < 1 then AX = 1
CAL_SKIPY2:
	pop	DS
	mov	SI,CX
	sub	SI,AX	; uy
	add	CX,AX	; ly
	mov	at_js_uy,SI
	mov	at_js_ly,CX

	mov	js_bexist,2

CAL_IGNORE:
	pop	SI
	pop	BP
	ret	2*3*2
endfunc		; }

func AT_JS_WAIT	; at_js_wait() {
	push	BP
	mov	BP,SP
	push	SI
	; 引き数
	p	= (RETSIZE+1)*2

	cmp	js_bexist,1
	mov	AX,1
	jl	short WAIT_SKIP

	push	js_bexist
	mov	js_bexist,2

	mov	SI,js_stat
WAIT_LOOP:
	_call	AT_JS_SENSE
	test	AX,AX
	jnz	short WAIT_STOP
	mov	AX,js_stat
	not	SI
	and	SI,AX
	test	SI,(JS_TRIG1 or JS_TRIG2)
	mov	SI,AX
	jz	short WAIT_LOOP

	s_push	DS
	s_pop	ES
	_les	BX,[BP+p]
	mov	AX,at_js_x1
	mov	ES:[BX],AX
	mov	AX,at_js_y1
	mov	ES:[BX+2],AX
	xor	AX,AX

WAIT_STOP:
	pop	js_bexist

WAIT_SKIP:
	pop	SI
	pop	BP
	ret	DATASIZE*2
endfunc		; }

END
