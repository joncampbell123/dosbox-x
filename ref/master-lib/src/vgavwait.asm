; master library - VGA
;
; Description:
;	VSYNC(VBLANK)開始待ち
;
; Function/Procedures:
;	void vga_vsync_wait(void) ;
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
;	VGA
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
;	93/12/19 Initial: vgavwait.asm/master.lib 0.22
;	95/ 2/21 生DOS以外も対処(事前にget_machineが実行されている場合のみ)

	.MODEL SMALL
	include func.inc

DOSBOX equ 8000h

USE_INT = 1

if USE_INT ne 0
	.DATA
	EXTRN vsync_Count1 : WORD	; 増加し続けるカウンタ1
	EXTRN vsync_OldMask : BYTE
	EXTRN Machine_State : WORD
endif

	.CODE

func VGA_VSYNC_WAIT	; vga_vsync_wait() {
if USE_INT ne 0
	test	Machine_State,DOSBOX
	jnz	short FORCE_RAW
	cmp	vsync_OldMask,0
	jnz	short USING_INT
endif

FORCE_RAW:
	; VBLANKの立ち上がりを待つ
	; VGAのばあい
	mov	DX,03dah	; VGA入力レジスタ1(カラー)
RUNNING:
	jmp	short $+2
	jmp	short $+2
	in	AL,DX
	test	AL,8
	jnz	short RUNNING
SLEEPING:
	jmp	short $+2
	jmp	short $+2
	in	AL,DX
	test	AL,8
	jz	short SLEEPING
	ret

if USE_INT ne 0
	; DOSで割り込みを使ってれば、port直接読まずに割り込みを使う
USING_INT:
	STI
	mov	AX,vsync_Count1
INT_WAIT:
	cmp	AX,vsync_Count1
	je	short INT_WAIT
	ret
endif
endfunc		; }

END
