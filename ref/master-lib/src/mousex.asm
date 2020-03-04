; master library - MOUSE - EXTERNAL
;
; Description:
;	外部マウスドライバの制御
;
; Function/Procedures:
;	int mousex_start( void ) ;
;	void mousex_end(void) ;
;	void mousex_setrect( int x1, int y1, int x2, int y2 ) ;
;	void mousex_moveto( int x, int y ) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
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
;	93/ 7/ 5 Initial: mousex.asm/master.lib 0.20
;	93/ 7/26 [M0.20] 入れるのやめた
;	93/11/29 [M0.22] やっぱり入れる
;	94/ 4/10 [M0.23] mousex_endで初期化はやめてイベントを消すようにした
;	94/ 5/27 [M0.23] 開始以前のマウスイベントルーチンのアドレスを待避

	.MODEL SMALL
	include func.inc

	.DATA

MOUSEX_NONE	equ 0
MOUSEX_NEC	equ 1
MOUSEX_MS	equ 2

	EXTRN mouse_Type:WORD
	EXTRN mouse_X:WORD
	EXTRN mouse_Y:WORD
	EXTRN mouse_Button:WORD
	EXTRN mouse_ScaleX:WORD
	EXTRN mouse_ScaleY:WORD
	EXTRN mouse_EventMask:WORD
	EXTRN mouse_EventRoutine:DWORD

	extrn Machine_State:WORD

	.DATA
lastMask DW	?
lastFunc DD	?

	.CODE

event_hook_ms proc far
	xchg	BX,DX	; x=DX, y=BX,  button=CX
	xchg	DX,CX

	mov	AH,AL
	and	AH,not 1
	add	AL,AH
	shl	AL,1
	shl	AL,1
	and	CL,3
	or	AL,CL
	jmp	short event_hook_merge
event_hook_ms endp

event_hook_nec proc far
	shl	BL,1
	rcl	BH,1	; bit 1=BR,  bit 0=BL
	and	BH,3
	mov	AH,BH
	mov	BX,DX
	mov	DX,CX
	mov	CL,AH
	cmp	CL,1
	jne	short NEC_0
	dec	CL
NEC_0:
	mov	AL,04h
	shl	AL,CL
	or	AL,AH

event_hook_merge:
	mov	SI,seg DGROUP
	mov	DS,SI
	mov	mouse_X,DX
	mov	mouse_Y,BX
	mov	AH,AL
	and	AH,3
	mov	byte ptr mouse_Button,AH

	mov	AH,0
	test	AX,mouse_EventMask
	jz	short NEC_RET

	mov	SI,AX
	CLD
	call	mouse_EventRoutine
NEC_RET:

	; abc
	test	Machine_State,010h	; PC/AT?
	jz	short rtc_refresh_done
RTC_INDEX	equ 70h		; RTC index port
RTC_DATA	equ 71h		; RTC data port
	mov	AL,0ch
	out	RTC_INDEX,AL		; AT
	in	AL,RTC_DATA	; clear interrupt request
	mov	AL,0ch
	out	RTC_INDEX,AL		; AT
	in	AL,RTC_DATA	; clear interrupt request
rtc_refresh_done:

	retf
event_hook_nec endp


func MOUSEX_START	; mousex_start() {
	mov	AX,8
	mov	mouse_ScaleX,AX
	mov	mouse_ScaleY,AX

	xor	CX,CX	; no call on all event
	mov	AX,20	; exchange interrupt sub routine
	int	33h
	mov	lastMask,CX
	mov	word ptr lastFunc,DX
	mov	word ptr lastFunc+2,ES

	xor	AX,AX
	int	33h
	test	AX,AX
	jz	short START_OK		; MOUSEX_NONE

	mov	AX,3
	int	33h
	cmp	AX,3
	mov	BX,MOUSEX_NEC
	mov	DX,offset event_hook_nec
	jne	short HOOK_EVENT
	inc	BX			; MOUSEX_MS
	mov	DX,offset event_hook_ms
HOOK_EVENT:
	mov	AX,12
	mov	CX,1fh	; call on all event
	push	CS
	pop	ES
	int	33h

	mov	AX,BX
START_OK:
	mov	mouse_Type,AX
	ret
endfunc		; }

func MOUSEX_END		; mousex_end() {
	mov	AX,12
	mov	CX,lastMask
	les	DX,lastFunc
	int	33h
	ret
endfunc		; }

	public mousex_snapshot
mousex_snapshot proc near
	pushf
	CLI
	mov	AX,3
	int	33h
	mov	mouse_X,CX
	mov	mouse_Y,DX

	cmp	mouse_Type,MOUSEX_NEC
	jl	short SNAP_IGNORE
	jne	short SNAP_DONE
	shr	AX,1
	rcl	BX,1	; bit 1=right  bit 0=left button

SNAP_DONE:
	and	BX,3
	mov	mouse_Button,BX
SNAP_IGNORE:
	popf
	ret
mousex_snapshot endp

func MOUSEX_SETRECT	; mousex_setrect(x1,y1,x2,y2) {
	push	BP
	mov	BP,SP
	; 引数
	x1 = (RETSIZE+4)*2
	y1 = (RETSIZE+3)*2
	x2 = (RETSIZE+2)*2
	y2 = (RETSIZE+1)*2

	mov	AX,7
	cmp	mouse_Type,MOUSEX_NEC
	jne	short SETRECT_MS
	mov	AX,10h
SETRECT_MS:
	mov	CX,[BP+x1]
	mov	DX,[BP+x2]
	int	33h

	inc	AX
	mov	CX,[BP+y1]
	mov	DX,[BP+y2]
	int	33h
	pop	BP
	call	mousex_snapshot
	ret	8
endfunc		; }

func MOUSEX_MOVETO	; mousex_moveto(x,y) {
	mov	BX,SP
	; 引数
	x = (RETSIZE+1)*2
	y = (RETSIZE+0)*2

	mov	AX,4
	mov	CX,SS:[BX+x]
	mov	DX,SS:[BX+y]
	int	33h
	call	mousex_snapshot
	ret	4
endfunc		; }

END
