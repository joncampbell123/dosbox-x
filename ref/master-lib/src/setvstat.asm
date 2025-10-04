; Hiper library
;
; Description:
;	DOS/V の video mode と付随情報を復元する。
;
; Procedures/Functions:
;	int restore_video_state( VIDEO_STATE *vmode ) ;
;
; Parameters:
;	vmode  ビデオ・モード構造体
;
; Returns:
;	1 (cy=0)  成功
;	0 (cy=1)  失敗
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;
; Compiler/Assembler:
;	OPTASM 1.6
;
; Author:
;	原形　のろ/V(H.Maeda)
;	SIVA / DSA
;
; Revision History:
;	93/10/ 6 Initial
;	93/12/27 VESA対応? by 恋塚
;	93/12/27 Initial: setvideo.asm/master.lib 0.22
;	94/ 3/12 [M0.23] 戻り値があるのにvoidって書いてあったのを訂正
;	94/03/23 DSPX L 対応(SIVA/DSA)
;	94/ 3/23 Initial: setvstat.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc
	include vgc.inc

	EXTRN VTEXT_START:CALLMODEL

	.DATA
	EXTRN Machine_State : WORD

	.CODE

func RESTORE_VIDEO_STATE	; restore_video_state() {
	push	BP
	mov	BP,SP
	push	DI
	vmode = (RETSIZE+1) * 2

	s_mov	AX,DS
	s_mov	ES,AX
	_les	DI,[BP+vmode]

	test	Machine_State,0010h	; PC/AT だったら
	jz	short FAILURE

	mov	BL, (VIDEO_STATE ptr ES:[DI]).mode
	mov	BH,0
IF 1
	jmp	short NORMAL
ELSE
	test	BH,BH
	jz	short NORMAL

VESA:
	mov	AX,4f02h	; VESA set video mode
	int	10h
	cmp	AX,4fh
	je	short SUCCESS
ENDIF

FAILURE:
	stc
	xor	AX,AX
	pop	DI
	pop	BP
	ret	(DATASIZE)*2

NORMAL:
	mov	AH,0		; set video mode
	mov	AL,BL
	int	10h

	test	Machine_State,1	; 0=日本語モード
	jnz	short SUCCESS

	mov	AL, (VIDEO_STATE ptr ES:[DI]).total_rows
	cmp	AL,25
	je	short WIDE_MODE
	mov	AL, (VIDEO_STATE ptr ES:[DI]).mode
	cmp	AL,3
	je	short LARGE_MODE
	cmp	AL,73h
	jne	short WIDE_MODE
LARGE_MODE:
	mov	AX, 1118h
	xor	BX, BX
	int	10h
WIDE_MODE:

SUCCESS:
	call	VTEXT_START		; 関連変数を読み取り直す
	mov	AX,1
	clc
	pop	DI
	pop	BP
	ret	(DATASIZE)*2
endfunc			; }

END
