; master library - (pf.lib)
;
; Description:
;	読み込みバッファへの充填と先頭バイトの読み込み
;
; Functions/Procedures:
;	int bfill(bf_t bf);
;
; Parameters:
;	bf	bファイルハンドル
;
; Returns:
;	-1	ファイル終端に達したので1バイトも読めなかった
;	0～255	バッファの先頭バイト(充填成功)
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
; BFILL.ASM         571 94-05-26   22:44
;	95/ 1/10 Initial: bfill.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE

func BFILL	; bfill() {
	push	BP
	mov	BP,SP
	push	DS

	;arg	bf:word
	bf	= (RETSIZE+1)*2

	mov	DS,[BP+bf]		; BFILE構造体のセグメント

	mov	BX,DS:[b_hdl]
	mov	CX,DS:[b_siz]
	lea	DX,DS:[b_buff]
	msdos	ReadHandle
	jc	short _error
	chk	AX
	jz	short _error

_getc:
	dec	AX
	mov	DS:[b_left],AX
	mov	DS:[b_pos],1
	mov	AL,DS:[b_buff]
	clr	AH

	pop	DS
	pop	BP
	ret	(1)*2

_error:
	clr	AX
	mov	DS:[b_left],AX
	dec	AX		; mov	AX,EOF

	pop	DS
	pop	BP
	ret	(1)*2
endfunc		; }

END
