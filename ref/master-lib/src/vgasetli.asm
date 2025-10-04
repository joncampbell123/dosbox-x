; master library - VGA
;
; Description:
;	VGAの表示ライン数の設定
;
; Function/Procedures:
;	void vga_setline( unsigned lines ) ;
;	procedure vga_setline( lines:Word ) ;
;
; Parameters:
;	lines:	表示ライン数(480以内)
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
;	恋塚昭彦
;
; Revision History:
;	93/12/25 Initial: vgasetli.asm/master.lib 0.22
;	94/ 4/10 [M0.23] BUGFIX 設定値が 1大きかった

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	graph_VramLines:WORD
	EXTRN	graph_VramWidth:WORD
	EXTRN	graph_VramWords:WORD

	.CODE

DC_PORT	equ	3d4h

;procedure vga_dc_modify( num, andval, orval: integer ) ;
; out: DX=DC_PORT
func VGA_DC_MODIFY	; vga_dc_modify() {
	push	BP
	mov	BP,SP
	; 引数
	num	= (RETSIZE+3)*2
	andval	= (RETSIZE+2)*2
	orval	= (RETSIZE+1)*2

	mov	DX,DC_PORT
	mov	AL,[BP+num]
	out	DX,AL
	inc	DX
	xchg	AL,AH
	in	AL,DX
	xchg	AL,AH
	dec	DX
	and	AH,[BP+andval]
	or	AH,[BP+orval]
	out	DX,AX
	pop	BP
	ret	6
endfunc			; }

func VGA_SETLINE	; vga_setline() {
	mov	BX,SP
	; 引数
	lines = (RETSIZE+0)*2
	mov	BX,SS:[BX+lines]
	mov	graph_VramLines,BX
	mov	AX,graph_VramWidth
	mul	BX
	shr	AX,1
	mov	graph_VramWords,AX

	dec	BX		; 設定値はひとつ少なくするのね

	mov	DX,DC_PORT
	mov	AL,12H
	mov	AH,BL		; b7 b6 b5 b4 b3 b2 b1 b0
	out	DX,AX

	mov	AX,7
	push	AX
	mov	AX,10111101b
	push	AX
	mov	AL,BH
	and	AL,1
	shl	AL,1		; .. b9 .. .. .. .. b8 ..

	and	BH,2
	mov	CL,5
	shl	BH,CL
	or	AL,BH
	push	AX
	call	VGA_DC_MODIFY

	ret	2
endfunc		; }

END
