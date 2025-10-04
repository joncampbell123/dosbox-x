; master library - MS-DOS - RSL
;
; Description:
;	RSL ( 常駐 synbolic link manager (c)恋塚 ) 制御
;
; Procedures/Functions:
;	int rsl_exist( void ) ;
;	int rsl_readlink( char * buf, const char * path ) ;
;	int rsl_linkmode( unsigned mode ) ;
;
; Parameters:
;	
;
; Returns:
;	rslが存在すれば		1
;	rslが存在しなければ	0
;	rsl_linkmodeの場合: 存在しないとき0,それ以外は読んだ値
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS (with RSL)
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
;	93/ 4/10 Initial: master.lib/rsl.asm
;	93/ 9/ 3 [M0.21] rsl_exist以外は、存在しないと飛んだ??bugfix
;	93/ 9/28 [M0.21] rsl_readlinkの修正
;

	.MODEL SMALL
	include func.inc

	.DATA
RSL_ID	db ":symlink:",0

	.CODE

SymLinkFunc	dd	0	; 存在すれば絶対offsetは0にならないよ

func RSL_EXIST		; rsl_exist() {
	mov	AH,4eh
	mov	DX,offset RSL_ID
	clc
	int	21h
	mov	AX,0
	mov	word ptr CS:SymLinkFunc,AX
	jc	short RSL_NOTEXIST
	mov	word ptr CS:SymLinkFunc,BX
	mov	word ptr CS:SymLinkFunc+2,ES
	mov	AX,1
RSL_NOTEXIST:
	ret
endfunc			; }

func RSL_READLINK	; rsl_readlink() {
	cmp	word ptr CS:SymLinkFunc,0
	je	short READ_NO

	push	BP
	mov	BP,SP
	; 引数
	buf	= (RETSIZE+1+DATASIZE)*2
	path	= (RETSIZE+1)*2

	push	SI
	push	DI

	_push	DS
     s_ <push	DS>
     s_	<pop	ES>
	_lds	SI,[BP+path]
	_les	DI,[BP+buf]
	mov	AX,0
	call	CS:SymLinkFunc
	sbb	AX,AX
	inc	AX
	_pop	DS
	pop	DI
	pop	SI

	pop	BP
	ret	DATASIZE*2*2
READ_NO:
	mov	AX,0
	ret	DATASIZE*2*2
endfunc			; }

func RSL_LINKMODE	; rsl_linkmode() {
	cmp	word ptr CS:SymLinkFunc,0
	je	short MODE_NO

	mov	BX,SP
	; 引数
	mode	= (RETSIZE+0)*2

	mov	DX,SS:[BX+mode]
	mov	AX,1
	call	CS:SymLinkFunc
	mov	AX,1
	ret
MODE_NO:
	mov	AX,0
	ret
endfunc			; }

END
