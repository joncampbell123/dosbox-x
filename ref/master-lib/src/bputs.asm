; master library - (pf.lib)
;
; Description:
;	文字列の書き出し
;
; Functions/Procedures:
;	int bputs(const char *s,bf_t bf);
;
; Parameters:
;	s	文字列
;	bf	bファイルハンドル
;
; Returns:
;	
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
;	buf+sizeが10000hを越えてはならない。
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
; BPUTS.ASM         504 94-05-30    1:43
;	95/ 1/10 Initial: bputs.asm/master.lib 0.23
;

	.model SMALL
	include func.inc

	.CODE
	EXTRN	BPUTC:CALLMODEL

func BPUTS	; bputs() {
	push	BP
	mov	BP,SP
	push	SI

	;arg	s:dataptr,bf:word
	s	= (RETSIZE+2)*2
	bf	= (RETSIZE+1)*2

	CLD
	mov	SI,[BP+s]

_loop:
    s_ <lodsb>			; AH<-0は省略
    l_ <mov	ES,[BP+s+2]>
    l_ <lods	byte ptr ES:[SI]>
	or	AL,AL
	jz	short _return

	push	AX
	push	[BP+bf]
	_call	BPUTC

	or	AH,AH
	jz	short _loop
	; EOF
	jmp	short _exit
	EVEN

_return:
	lea	AX,[SI-1]
	sub	AX,[BP+s]

_exit:
	pop	SI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc		; }

END
