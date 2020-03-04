; master library - (pf.lib)
;
; Description:
;	1バイト読み込み関数
;
; Functions/Procedures:
;	int pfgetc(pf_t pf)
;
; Parameters:
;	
;
; Returns:
;	
;
; Subroutines:
;	以下は内部関数
;	※引き数は、ESレジスタに入れること
;	int pfgetc1(pf_t pf) 圧縮タイプ1用のpfgetc()
;	int pfgetx0(pf_t pf) 暗号なしのときの内部1byte読み出し関数
;	int pfgetx1(pf_t pf) 暗号を復号化するときの内部1byte読み出し関数
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
; PFGETC.ASM       1,751 94-09-17   23:31
;	95/ 1/10 Initial: pfgetc.asm/master.lib 0.23
;	95/ 1/19 MODIFY: 内部1バイト読み込みルーチンをレジスタ渡しに変更

	.186
	.model SMALL
	include func.inc
	include pf.inc

	.CODE
	EXTRN	BGETC:CALLMODEL

func PFGETC	; pfgetc() {
	push	BP
	mov	BP,SP

	;arg	pf:word
	pf	= (RETSIZE+1)*2

	mov	ES,[BP+pf]		; PFILE構造体のセグメント
	call	word ptr ES:[pf_getc]

	pop	BP
	ret	(1)*2
endfunc		; }


;		圧縮タイプ1用の pfgetc()
	; 圧縮方式: 同じデータが2バイト並んだら、続くバイトが長さ。
	;           256バイト以上ならば、さらに同じバイト+長さで連続できる。
	public PFGETC1
PFGETC1 proc near	; pfgetc1() {
	cmp	ES:[pf_cnt],0
	jz	short PFGETC1_getx
	dec	ES:[pf_cnt]
	add	word ptr ES:[pf_loc],1
	adc	word ptr ES:[pf_loc + 2],0
	mov	AX,ES:[pf_ch]
	ret

PFGETC1_getx:
	call	word ptr ES:[pf_getx]		; 新しいバイトが
	test	AH,AH
	jnz	short PFGETC1_return

	cmp	AX,ES:[pf_ch]		; 直前のバイトと同じ値なら
	mov	ES:[pf_ch],AX
	jne	short PFGETC1_return

	push	AX
	call	word ptr ES:[pf_getx]		; くり返しだ。続くバイトが
	test	AH,AH
	jnz	short PFGETC1_return1
	mov	ES:[pf_cnt],AX		; 長さね。
	sub	word ptr ES:[pf_loc],1
	sbb	word ptr ES:[pf_loc + 2],0
PFGETC1_return1:
	pop	AX

PFGETC1_return:
	ret
PFGETC1	endp		; }


;		暗号なしのときの内部1byte読み出し関数
	public PFGETX0
PFGETX0 proc near	; pfgetx0() {
	mov	AX,word ptr ES:[pf_read]
	mov	DX,word ptr ES:[pf_read + 2]
	cmp	DX,word ptr ES:[pf_size + 2]
	jb	short PFGETX0_getc
	ja	short PFGETX0_eof
	cmp	AX,word ptr ES:[pf_size]
	jb	short PFGETX0_getc
PFGETX0_eof:
	mov	AX,EOF
	ret

PFGETX0_getc:
	add	AX,1
	adc	DX,0
	mov	word ptr ES:[pf_read],AX
	mov	word ptr ES:[pf_read + 2],DX

	add	word ptr ES:[pf_loc],1
	adc	word ptr ES:[pf_loc + 2],0

	push	ES
	push	ES:[pf_bf]
	_call	BGETC
	pop	ES

	ret
PFGETX0	endp	; }


;		暗号を復号化するときの内部1byte読み出し関数
	public PFGETX1
PFGETX1 proc near	; pfgetx1() {
	call	PFGETX0
	or	AH,AH		; == EOF?
	jnz	short PFGETX1_return

	;	復号化
	xor	AL,ES:[pf_key]
	ror	AL,4

PFGETX1_return:
	ret
PFGETX1	endp	; }

END
