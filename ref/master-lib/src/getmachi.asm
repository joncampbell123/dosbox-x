; Hiper library
;
; Description:
;	起動したマシン情報を得る。
;
; Procedures/Functions:
;	int get_machine( void );
;
; Parameters:
;
; Returns:
;          bit 15 - 8 : 0 : (reserved)
;	   bit	 54   :
;		 00   : unknown machine
;		 01   : PC/AT or compatible
;		 10   : PC-9801
;		 11   : unknown machine(ありえない)
;	   bit   3210 : bit 4 が 1 のとき
;		 000x : PS/55
;		 001x : DOS/V
;		 010x : AX
;		 011x : J3100
;		 100x : DR-DOS
;		 101x : MS-DOS/V
;		 110x : (reserved)
;		 111x : (reserved)
;		 xxx0 : Japanese mode
;		 xxx1 : English mode
;	   bit  83210 : bit 5 が 1 のとき
;		xxxx0 : DeskTop
;		xxx0x : NEC	#0.07 以降
;		xxx1x : EPSON	#0.07 以降
;		x00xx : Normal mode only    (400 lines)
;		x01xx : MATE mode supported (480 lines)
;		x1xxx : High-resolustion mode
;
;
; Binding Target:
;	Microsoft-C / Turbo-C
;
; Running Target:
;	PC-9801, PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	PC-9801, PC/AT 以外のマシンの返り値について保証できません。
;	FM-R では、暴走するらしいっす(^_^;
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	のろ/V(H.Maeda)
;
; Revision History:
;	93/ 7/19 Initial: Hiper.lib
;	93/07/29 add PC-9821 check
;	93/08/10 add PC-98 color/mono check ?
;		 & _get_machine -> get_machine
;	93/08/25 98 MATE check bugfix
;	93/09/19 ret 処理が... (^_^;
;	93/10/06 si を壊していた(^_^;
;	93/10/22 Mono&Color チェック廃止、NEC, EPSON チェック追加
;		 また、EPSON note もチェック可能
;	93/11/ 3 AX check で、0040:00e0 ～を
;		 16 byte から 12 byte check に変更
;	93/12/11 Initial: getmachi.asm/master.lib 0.22 (from hiper.lib)
;	94/01/05 get_machine() を、get_machine_at()、get_machine_98() に分割
;		 それに伴い、それぞれ、getmachi.asm、getmacat.asm、gatmac98.asm
;		 にファイルを分離。
;	95/ 2/21 各機種部分へjumpしていたのをcallに変更し、戻ってきたら
;		 DOS BOX判定をするように追加。
;	95/ 3/ 8 get_machine_dosbox(getmacdb.asm)に分離。このため
;		callにしていたのをjumpに戻した。


	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN	Machine_State:WORD		; machine.asm

FMR	EQU	01000000b

	.CODE

	EXTRN	CHECK_MACHINE_FMR:CALLMODEL	; getmacfm.asm
	EXTRN	GET_MACHINE_98:CALLMODEL	; getmac98.asm
	EXTRN	GET_MACHINE_AT:CALLMODEL	; getmacat.asm

func GET_MACHINE	; get_machine(void)
	_call	CHECK_MACHINE_FMR
	jz	short FMR_RET
	mov	ah,0fh
	int	10h	; get text mode (DOS/V)
	cmp	ah,0fh
	je	GET_MACHINE_98
	jmp	GET_MACHINE_AT
FMR_RET:
	mov	AX,FMR
	ret
endfunc

END
