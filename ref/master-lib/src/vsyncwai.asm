; master library - PC-9801
;
; Description:
;	VSYNC(VBLANK)開始待ち
;
; Function/Procedures:
;	void vsync_wait(void) ;
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
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	割り込み禁止状態で呼び出すとハングアップの恐れがあります。
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
;	92/11/21 Initial: vsync.asm
;	93/ 8/ 8 Initial: vsyncwai.asm/master.lib 0.20

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN vsync_Count1 : WORD	; 増加し続けるカウンタ1
	EXTRN vsync_OldMask : BYTE

	.CODE

func VSYNC_WAIT	; vsync_wait() {
	cmp	vsync_OldMask,0
	jnz	short USING_INT

	; VBLANKの立ち上がりを待つ
RUNNING:
	jmp	short $+2
	jmp	short $+2
	in	AL,0a0h
	test	AL,20h
	jnz	short RUNNING
SLEEPING:
	jmp	short $+2
	jmp	short $+2
	in	AL,0a0h
	test	AL,20h
	jz	short SLEEPING
	ret

	; Windowsではポート監視だと正しく読み取れないため、
	; 割り込みを使っていればそちらを優先する。
USING_INT:
	mov	AX,vsync_Count1
INT_WAIT:
	cmp	AX,vsync_Count1
	je	short INT_WAIT
	ret
endfunc		; }

END
