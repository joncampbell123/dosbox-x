; master library - (pf.lib)
;
; Description:
;	ファイルポインタの相対移動
;
; Functions/Procedures:
;	int bseek(bf_t bf, long offset);
;
; Parameters:
;	bf	bファイルポインタ
;	offset	相対移動量
;
; Returns:
;	0  成功
;	-1 失敗
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 186
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
;	iR
;	恋塚昭彦
;
; Revision History:
; BSEEK.ASM         694 94-06-01    8:40
;	95/ 1/10 Initial: bseek.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func BSEEK	; bseek() {
	push	BP
	mov	BP,SP

	;arg	bf:word,ofst:dword
	bf	= (RETSIZE+3)*2
	ofst	= (RETSIZE+1)*2

	mov	ES,[BP+bf]		; BFILE構造体のセグメント

	mov	AX,ES:[b_left]
	mov	DX,[BP+ofst]
	mov	CX,[BP+ofst+2]
	or	CX,CX
	jnz	short _outbuf
	cmp	DX,AX
	ja	short _outbuf

_inbuf:
	sub	ES:[b_left],DX
	add	ES:[b_pos],DX
	clr	AX
	pop	BP
	ret	(3)*2
	EVEN

_outbuf:
	; ファイルポインタを移動(カレントから)
	mov	BX,ES:[b_hdl]
	sub	DX,AX
	sbb	CX,0

	msdos	MoveFilePointer,1

	sbb	AX,AX		; mov AX,carry
	mov	ES:[b_left],AX
	pop	BP
	ret	(3)*2
endfunc		; }

END
