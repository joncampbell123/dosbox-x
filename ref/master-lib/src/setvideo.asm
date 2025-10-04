; Hiper library
;
; Description:
;	DOS/V の video mode を設定する。
;
; Procedures/Functions:
;	int set_video_mode( unsigned video ) ;
;
; Parameters:
;	video  ビデオモード
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
;	のろ/V(H.Maeda)
;
; Revision History:
;	93/10/ 6 Initial
;	93/12/27 VESA対応? by 恋塚
;	93/12/27 Initial: setvideo.asm/master.lib 0.22
;	94/ 3/12 [M0.23] 戻り値があるのにvoidって書いてあったのを訂正


	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN Machine_State : WORD

	.CODE

func SET_VIDEO_MODE	; set_video_mode() {
	mov	BX,SP
	video	= (RETSIZE+0)*2

	mov	BX,SS:[BX+video]

	test	Machine_State,0010h	; PC/AT だったら
	jz	short FAILURE

	test	BH,BH
	jz	short NORMAL

VESA:
	mov	AX,4f02h	; VESA set video mode
	int	10h
	cmp	AX,4fh
	je	short SUCCESS

FAILURE:
	stc
	xor	AX,AX
	ret	2

NORMAL:
	mov	AH,0		; set video mode
	mov	AL,BL
	int	10h
SUCCESS:
	mov	AX,1
	clc
	ret	2
endfunc			; }

END
