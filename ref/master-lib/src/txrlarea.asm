; master library - text
;
; Description:
;	テキスト画面のスクロール範囲の指定
;
; Function/Procedures:
;	void text_roll_area( int x1, int y1, int x2, int y2 ) ;
;
; Parameters:
;	x1,y1	左上の座標( (0,0)=左上隅 )
;	x2,y2	右下の座標( (79,24)= 80x25モードの右下隅 )
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801, PC/AT
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	範囲検査してにゃい
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/20 Initial
;	94/ 4/ 9 [M0.23] TextVramWidth対応

	.MODEL SMALL

	include func.inc

	.DATA
	EXTRN text_RollTopOff:WORD
	EXTRN text_RollHeight:WORD
	EXTRN text_RollWidth:WORD
	EXTRN TextVramWidth:WORD

	.CODE

func TEXT_ROLL_AREA	; text_roll_area() {
	push	BP
	mov	BP,SP
	; 引数
	x1	= (RETSIZE+4)*2
	y1	= (RETSIZE+3)*2
	x2	= (RETSIZE+2)*2
	y2	= (RETSIZE+1)*2

	mov	AX,[BP+y2]
	mov	BX,[BP+y1]
	sub	AX,BX	; text_RollHeight = y2 - y1
	mov	text_RollHeight,AX

	mov	AX,TextVramWidth
	imul	BX
	mov	BX,[BP+x1]
	add	AX,BX
	shl	AX,1	; text_RollTopOff = (y1 * TextVramWidth + x1) * 2
	mov	text_RollTopOff,AX

	mov	AX,[BP+x2]
	sub	AX,BX
	inc	AX	; text_RollWidth = x2 - x1 + 1
	mov	text_RollWidth,AX

	pop	BP
	ret 8
endfunc			; }

END
