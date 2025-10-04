; master library - Pi - free
;
; Description:
;	読み込まれたPi情報を開放する
;
; Function/Procedures:
;	void graph_pi_free( PiHeader * header, void far * image ) ;
;
; Parameters:
;	header	すでに画像(とコメント)が読み込まれたPiヘッダ
;	image	画像データの先頭アドレス(NULLなら無視)
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	8086
;
; Requiring Resources:
;	CPU: 8086
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
;	恋塚昭彦
;
; Revision History:
;	93/11/24 Initial: grppifre.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc
	EXTRN	HMEM_FREE:CALLMODEL

	.CODE

PiHeader STRUC
comment_offset	dw	?
comment_segment	dw	?
commentlen	dw	?
_mode		db	?
_n		db	?
_m		db	?
_plane		db	?
_machine	db	4 dup (?)
maexlen		dw	?
maex_offset	dw	?
maex_segment	dw	?
_xsize		dw	?
_ysize		dw	?
_palette	db	48 dup (?)
PiHeader ENDS


func GRAPH_PI_FREE	; graph_pi_free() {
	push	BP
	mov	BP,SP
	; 引数
	header = (RETSIZE+3)*2
	image_segment = (RETSIZE+2)*2
	image_offset = (RETSIZE+1)*2

	xor	CX,CX

    s_ <push	DS>
    s_ <pop	ES>
	_les	BX,[BP+header]
	mov	AX,(PiHeader ptr ES:[BX]).comment_segment
	test	AX,AX
	jz	short NO_COMMENT
	push	AX
	call	HMEM_FREE
	mov	(PiHeader ptr ES:[BX]).commentlen,CX
	mov	(PiHeader ptr ES:[BX]).comment_segment,CX
	mov	(PiHeader ptr ES:[BX]).comment_offset,CX

NO_COMMENT:
	mov	AX,(PiHeader ptr ES:[BX]).maex_segment
	test	AX,AX
	jz	short NO_MACHINEINFO
	push	AX
	call	HMEM_FREE
	mov	(PiHeader ptr ES:[BX]).maexlen,CX
	mov	(PiHeader ptr ES:[BX]).maex_segment,CX
	mov	(PiHeader ptr ES:[BX]).maex_offset,CX

NO_MACHINEINFO:
	mov	AX,[BP+image_segment]
	test	AX,AX
	jz	short NO_IMAGE
	push	AX
	call	HMEM_FREE

NO_IMAGE:

	pop	BP
	ret	(DATASIZE+2)*2
endfunc		; }

END
