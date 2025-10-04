; master library - MS-DOS
;
; Description:
;	パス名の中のファイル名部分を見つける
;
; Function/Procedures:
;	char * file_basename( const char * pathname ) ;
;
; Parameters:
;	char * pathname	パス名
;
; Returns:
;	char *		ベース名のある(べき)地点の先頭アドレス
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
;	92/11/17 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func FILE_BASENAME
	mov	BX,SP
	push	SI
	_push	DS

	; 引数
	path	= RETSIZE*2

	_lds	SI,SS:[BX+path]

	mov	AX,DS
	or	AX,SI
	jz	short EXIT	; NULLならNULLだ

	mov	CX,-1		; CX = kanji flag ( 0 = kanji )
	mov	BX,SI

	lodsb
	or	AL,AL
	jz	short EXIT
RLOOP:
	jcxz	short REVERT
	test	AL,0e0h
	jns	short CHECKSEP	; 大ざっぱな漢字検査
	jp	short CHECKSEP	; 80..9f, e0..ff
REVERT:
	not	CX
	jmp	short ENDLOOP
	EVEN
CHECKSEP:
	cmp	AL,':'
	je	short COPY
	cmp	AL,'/'
	je	short COPY
	cmp	AL,'\'
	jne	short ENDLOOP
COPY:	mov	BX,SI
ENDLOOP:lodsb
	or	AL,AL
	jnz	short RLOOP

EXIT:	_mov	DX,DS
	mov	AX,BX
	_pop	DS
	pop	SI
	ret	DATASIZE*2
endfunc

END
