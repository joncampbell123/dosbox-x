; master library - vtext
;
; Description:
;	テキスト関連の情報取得
;
; Function/Procedures:
;	int vtext_width( void ) ;
;	int vtext_height( void ) ;
;	int vtext_font_height( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	
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
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	のろ/V
;
; Revision History:
;	93/ 7/22 Initial
;	93/08/10 関数名から _ を取る
;	93/08/28 height -> width
;	93/10/ 8 get_text_*()  --->  get_vtext_*()
;	94/01/16 関数名称変更
;		 get_text_width -> vtext_width
;		 get_text_lines -> vtext_lines
;		 get_font_height -> vtext_font_height
;		 get_text_lines -> vtext_height
;	94/ 4/ 9 Initial: vtesc.asm/master.lib 0.23

	.MODEL SMALL
	include func.inc

	.DATA

	EXTRN Machine_State : WORD

	.CODE

; テキスト画面の行数を得る
; ※ファンクションキー表示がされていると１だけ少ない値になる。
func VTEXT_HEIGHT
	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:[0484h]
	inc	AX
	ret
endfunc

func VTEXT_WIDTH
	xor	AX,AX
	mov	ES,AX
	mov	AX,ES:[044Ah]
	ret
endfunc

func VTEXT_FONT_HEIGHT
	xor	AX,AX
	mov	ES,AX
	mov	AL,ES:[0485h]
	ret
endfunc

END
