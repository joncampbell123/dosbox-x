; master library module
;
; Description:
;	テキスト画面処理の開始/終了
;
; Functions/Procedures:
;	void text_start( void ) ;
;	void text_end( void ) ;
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
;	text_start:ハイレゾモードかノーマルモードか判定して
;		TextVramSegに適正値を設定します。
;		これによって text_*関数がハイレゾモードでも正しく
;		動作するようになります。
;
;	text_end:なにもしません(笑)
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚(恋塚昭彦)
;
; Revision History:
;	93/ 4/18 Initial: txstart.asm/master.lib 0.16
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramSeg:WORD

	.CODE

func TEXT_START		; text_start() {
	xor	AX,AX
	mov	ES,AX
	mov	AH,0a0h
	test	byte ptr ES:[0501H],8
	jz	short NORMAL
	mov	AH,0e0h
NORMAL:
	mov	TextVramSeg,AX
	ret
endfunc			; }

func TEXT_END
	ret
endfunc

END
