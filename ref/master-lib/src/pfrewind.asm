; master library - (pf.lib)
;
; Description:
;	ファイルポインタを先頭に戻す
;
; Functions/Procedures:
;	void pfrewind(pf_t pf);
;
; Parameters:
;	pf	pファイルハンドル
;
; Returns:
;	none
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
; PFREWIND.ASM         462 94-09-17   23:18
;	95/ 1/10 Initial: pfrewind.asm/master.lib 0.23

	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BSEEK_:CALLMODEL

func PFREWIND	; pfrewind() {
	push	BP
	mov	BP,SP

	;arg	pf:word
	pf	= (RETSIZE+1)*2

	mov	ES,[BP+pf]		; PFILE構造体のセグメント

	xor	AX,AX			; AX = 0
	mov	ES:[pf_cnt],AX
	mov	ES:[pf_ch],EOF
	mov	word ptr ES:[pf_read],AX
	mov	word ptr ES:[pf_read + 2],AX
	mov	word ptr ES:[pf_loc],AX
	mov	word ptr ES:[pf_loc + 2],AX

	;call	BSEEK_ Pascal,ES:[pf_bf],ES:[pf_home],AX
	push	ES:[pf_bf]
	push	word ptr ES:[pf_home+2]
	push	word ptr ES:[pf_home]
	push	AX
	_call	BSEEK_

	pop	BP
	ret	(1)*2
endfunc	; }

END

