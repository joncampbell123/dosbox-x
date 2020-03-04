; Master Library - Graphics
;
; Description:
;	グラフィック画面の表示状態を得る
;
; Functions:
;	int graph_shown( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	int	0 = 非表示
;		1 = 表示
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/24 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func GRAPH_SHOWN
	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:054ch
	rol	AL,1
	and	AL,1
	ret
endfunc

END
