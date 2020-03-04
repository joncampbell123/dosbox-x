; master library - PC98 - MSDOS
;
; Description:
;	テキストカーソル位置を指定する( NEC MS-DOS拡張ファンクション使用 )
;
; Function/Procedures:
;	void text_locate( unsigned x, unsigned y ) ;
;
; Parameters:
;	unsigned x	カーソルの横位置( 0 ～ 79 )
;	unsigned y	カーソルの縦位置( 0 ～ 24 )
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
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
;	92/11/15 Initial
;	92/11/24 MS-DOS workarea更新
;	92/12/02 ↑なに無駄なことやってんだよ

	.model SMALL
	include func.inc
	.CODE

func TEXT_LOCATE
	CLI
	add	SP,retsize*2
	pop	AX	; y
	pop	DX	; x
	sub	SP,(retsize+2)*2
	STI

	mov	DH,AL
	mov	AH,3
	mov	CL,10h
	int	0DCh
	ret	4
endfunc

END
