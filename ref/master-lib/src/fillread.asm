; master library - MS-DOS
;
; Description:
;	ファイルからの読み込み(long version)
;
; Function/Procedures:
;	unsigned long file_lread( char far * buf, unsigned long bsize ) ;
;
; Parameters:
;	char far * buf	読み込み先( NULLならば、bsize分読み捨てる )
;	unsigned long bsize 最大読み取り長(バイト数,
;			0 ならば file_Bufferを必要ならば更新するだけ )
;
; Returns:
;	読み込んだバイト数
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	file_EOFは、1バイトも読めなかった時だけ設定される。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/11 Initial
;	93/ 2/ 2 bugfix

	.MODEL SMALL
	include func.inc
	EXTRN FILE_READ:CALLMODEL

	.CODE
func FILE_LREAD
	push	BP
	mov	BP,SP

	xor	BX,BX
	push	BX
	push	BX

	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+3)*2
	bsize	= (RETSIZE+1)*2
	readlen = -4

	mov	SI,[BP+buf]	;
	mov	DI,[BP+buf+2]	; DI:SI
	mov	CL,4
	mov	AX,SI
	shr	AX,CL
	add	DI,AX
	and	SI,0fh		; DI:SIは正規化された huge pointerになった

	cmp	word ptr [BP+bsize+2],BX
	je	short READLAST
READLOOP:
	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_READ
	test	AX,AX
	jns	short FINISH
	add	[BP+readlen],AX
	adc	word ptr [BP+readlen+2],0
	add	DI,800h

	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_READ
	test	AX,AX
	jns	short FINISH
	add	[BP+readlen],AX
	adc	word ptr [BP+readlen+2],0
	add	DI,800h

	dec	word ptr [BP+bsize+2]
	jne	short READLOOP
READLAST:

	push	DI
	push	SI
	push	word ptr [BP+bsize]
	call	FILE_READ
FINISH:
	xor	DX,DX
	add	AX,[BP+readlen]
	adc	DX,[BP+readlen+2]

	pop	DI
	pop	SI
	mov	SP,BP
	pop	BP
	ret	8
endfunc

END
