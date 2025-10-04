; hiper library
;
; Description:
;	PC/AT の video mode を得る
;
; Procedures/Functions:
;	unsigned get_video_mode( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	ビデオモード (AT互換機でなければ 0 が返る)
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
;	のろ/V
;
; Revision History:
;	93/07/21 Initial
;	93/08/10 関数名から _ を取る
;	93/12/27 VESA対応? by 恋塚
;	93/12/27 Initial: setvideo.asm/master.lib 0.22

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN Machine_State : WORD

	.CODE

func GET_VIDEO_MODE	; get_video_mode() {
	xor	AX,AX
	test	Machine_State,0010h	; PC/AT だったら
	jz	short FAILURE

	test	Machine_State,1	; 英語モード?
	jz	short OLDTYPE	; 日本語なら無条件に普通のBIOSで取る
	mov	AX,4f03h	; VESA get video mode
	int	10h
	cmp	AX,4fh
	jne	short OLDTYPE
	mov	AX,BX
	cmp	AX,100h		; VESAで100h以上なら確定, 未満なら取り直し
	jnc	short SUCCESS
OLDTYPE:
	mov	AH,0fh
	int	10h
	mov	AH,0
SUCCESS:
FAILURE:
	ret
endfunc			; }

END
