; master library - MS-DOS
;
; Description:
;	ファイルに書き込む(long version)
;
; Function/Procedures:
;	int file_lwrite( const char far * buf, unsigned long bsize ) ;
;
; Parameters:
;	char far * buf		書き込むデータの先頭アドレス
;	unsigned long bsize	書き込むバイト数(0は禁止)
;
; Returns:
;	1 = 成功
;	0 = 失敗
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
;	92/12/11 Initial
;	93/ 2/ 2 bugfix

	.MODEL SMALL
	include func.inc
	EXTRN FILE_WRITE:CALLMODEL

	.CODE

func FILE_LWRITE
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	buf	= (RETSIZE+3)*2
	bsize	= (RETSIZE+1)*2

	mov	SI,[BP+buf]	;
	mov	DI,[BP+buf+2]	; DI:SI
	mov	CL,4
	mov	AX,SI
	shr	AX,CL
	add	DI,AX
	and	SI,0fh		; DI:SIは正規化された huge pointerになった

	cmp	word ptr [BP+bsize+2],0
	je	short READLAST
READLOOP:
	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_WRITE
	test	AX,AX
	jz	short FINISH
	add	DI,800h

	push	DI
	push	SI
	mov	AX,8000h
	push	AX
	call	FILE_WRITE
	test	AX,AX
	jz	short FINISH
	add	DI,800h

	dec	word ptr [BP+bsize+2]
	jne	short READLOOP
READLAST:

	push	DI
	push	SI
	push	word ptr [BP+bsize]
	call	FILE_WRITE
FINISH:

	pop	DI
	pop	SI
	pop	BP
	ret	8
endfunc

END
