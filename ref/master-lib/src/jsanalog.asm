; master library - 
;
; Description:
;	アナログジョイスティックの読み取り
;
; Function/Procedures:
;	int js_analog( int player, unsigned char astat[4] ) ;
;
; Parameters:
;	
;
; Returns:
;	
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: 
;
; Notes:
;	
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
;	93/ 6/20 Initial:jsanalog.asm/master.lib 0.19

	.MODEL SMALL
	include func.inc

	.DATA?

	EXTRN js_bexist:WORD
	EXTRN js_saj_port:WORD

	.CODE
	EXTRN SOUND_O : NEAR

READ_PULSE equ	2000

func JS_ANALOG	; {
	push	BP
	mov	BP,SP
	sub	SP,10
	push	DI

	; 引数
	player	= (RETSIZE+1+DATASIZE)*2
	astat	= (RETSIZE+1)*2

	; ローカル変数
	temp = -10

;	0	A B C D
;	1	E2 E1 START SELECT
;	2	ch0 high
;	3	ch1 high
;	4	ch2 high
;	5	ch3 high
;	6	ch0 low
;	7	ch1 low
;	8	ch2 low
;	9	ch3 low
;	10	A~ B~ A_ B_	; 読んでないけど A~B~はstick, A_B_は板のABね
;	11			; 読んでないけど

	pushf
	CLI

	mov	AX,[BP+player]	; 1=player1,  2=player2
	dec	AX
	cmp	AX,2
	jae	short HOP_ERROR

SAJ:
	mov	DX,js_saj_port
	test	DX,DX
	je	short JOYSTICK

	mov	CL,AL
	shl	AX,1	; BX = read port number
	mov	BX,AX
	add	BX,DX

	mov	AL,10h
	shl	AL,CL
	out	DX,AL
	mov	CX,1		; パルスの長さ
SAJDELAY:
	loop	short SAJDELAY
	mov	AL,0
	out	DX,AL
	mov	DX,BX

	jmp	short SENSE_START
	EVEN
HOP_ERROR:
	jmp	ERROREXIT
	EVEN

JOYSTICK:
	mov	CX,220		; パルスの長さ
	mov	AH,10

	cmp	js_bexist,0
	je	short ERROREXIT

	neg	AX
	sbb	BX,BX
	and	BX,40h
	or	BX,0fb0h
	call	SOUND_O		; ******** まずパルスを送るのね

JSDELAY:
	loop	short JSDELAY

	mov	AL,BL
	xor	AL,30H
	out	DX,AL

	mov	DX,188h
	mov	AL,0eh
	out	DX,AL		; ******** さーてパルスは送ったから
	inc	DX		; 読み込みを開始するかあ
	inc	DX


SENSE_START:
	mov	AH,5		; 5 バイト繰り返す( 10nibble )
	xor	DI,DI

SENSE_LOOP:
	mov	CX,READ_PULSE
WAIT1:
	in	AL,DX
	dec	CX
	jz	short ERROREXIT		; タイムアウト検査

	test	AL,30h
	jnz	short WAIT1		;どっちも 0になるまで待つ

		; なったら下位4bitを格納してポインタを次に
	and	AL,0fh
	mov	[BP+temp+DI],AL
	inc	DI

	mov	CX,READ_PULSE
WAIT2:
	mov	BH,0eh
	in	AL,DX
	dec	CX
	jz	short ERROREXIT

	test	AL,10h
	jz	short WAIT2		;1になるまで
	test	AL,20h
	jnz	short WAIT2		;0のまま

		; なったら下位4bitを格納してポインタを次に
	and	AL,0fh
	mov	[BP+temp+DI],AL
	inc	DI

	dec	AH
	jnz	short SENSE_LOOP

	popf		; 割り込み禁止を解除

	; 読み取りが終わったので、結果を格納する
	xor	AX,AX
	mov	CL,4
	_les	BX,[BP+astat]

	xor	DI,DI
STORE_LOOP:
	mov	AL,[BP+temp+2+DI]
	shl	AL,CL
	or	AL,[BP+temp+6+DI]
 l_	<mov	ES:[BX+DI],AL>
 s_	<mov	[BX+DI],AL>
	inc	DI
	cmp	DI,3
	jl	short STORE_LOOP

	mov	AX,[BP+temp]
	shl	AL,CL
	or	AL,AH
	not	AL
 l_	<mov	ES:[BX+DI],AL>
 s_	<mov	[BX+DI],AL>

	mov	AX,1
EXIT:
	pop	DI
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
	EVEN

ERROREXIT:
	popf
;	xor	AX,AX
	jmp	short	EXIT

endfunc		; }

END
