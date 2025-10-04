; master library - BGM
;
; Description:
;
;
; Function/Procedures:
;	void _bgm_timer_init(void);
;	void _bgm_timer_finish(void);
;
; Parameters:
;	none
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V, PC/AT
;
; Requiring Resources:
;	CPU: V30
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
;	femy(淀  文武)		: オリジナル・C言語版
;	steelman(千野  裕司)	: アセンブリ言語版
;
; Revision History:
;	93/12/19 Initial: b_timer.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	95/ 2/23 [M0.22k] RTC割り込みマネージャrtc_int_set使用

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc
	EXTRN	DOS_SETVECT:CALLMODEL

	.DATA
	EXTRN glb:WORD		;SGLB
	EXTRN timerorg:DWORD
	EXTRN Machine_State:WORD

	.CODE
	EXTRN _BGM_TIMERHOOK:far
	EXTRN rtc_int_set:NEAR		; in ATRTCMOD.ASM
RTC_SLOT_BGM		equ 0		; BGM RTC割り込みスロット番号

func _BGM_TIMER_INIT	; _bgm_timer_init() {
	test	Machine_State,10h	; PC/AT
	jnz	short INIT_PCAT

	;マスクレジスタ保存
	in	AL,IMR
	mov	AH,0
	mov	glb.simr,AX

	CLI

	;割り込みベクタの保存&書き換え
	push	8
	push	seg _BGM_TIMERHOOK
	push	offset _BGM_TIMERHOOK
	call	DOS_SETVECT
	mov	word ptr timerorg+2,DX
	mov	word ptr timerorg,AX
	;タイマインタラプト初期設定
	mov	AL,36h
	out	TIMER_SET,AL		; 98
	mov	AX,glb.tval
	out	TIMER_CNT,AL		; 98
	mov	AL,AH
	out	TIMER_CNT,AL		; 98
	;マスク解除
	mov	AL,byte ptr glb.simr
	and	AL,not TIMERMASK
	out	IMR,AL			; 98

	STI
	ret

INIT_PCAT:
	mov	AX,offset _BGM_TIMERHOOK
	mov	BX,RTC_SLOT_BGM
	call	rtc_int_set
	ret
endfunc			; }

func _BGM_TIMER_FINISH	; _bgm_timer_finish() {
	CLI
	test	Machine_State,10h	; PC/AT
	jnz	short FIN_PCAT

	;割り込みベクタを元に戻す
	push	8
	push	word ptr timerorg+2
	push	word ptr timerorg
	call	DOS_SETVECT
	;マスクレジスタを元に戻す
	mov	AL,byte ptr glb.simr
	out	IMR,AL			; 98

	STI
	ret

FIN_PCAT:
	mov	AX,0
	mov	BX,RTC_SLOT_BGM
	call	rtc_int_set
	ret
endfunc			; }


END
