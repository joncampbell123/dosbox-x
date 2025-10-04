; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	int bfnt_extend_header_analysis( int handle, BFNT_HEADER * header ) ;
;
; Parameters:
;	
;
; Returns:
;	
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
;	far未対応
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: bfthdran.asm 0.03 93/01/19 17:07:10 Kazumi Rel $
;
;	93/ 3/10 Initial: master.lib <- super.lib 0.22b
;	94/ 1/ 3 [M0.22] bugfix(透明色が存在するときにsmemが開放されなかった)
;

	.MODEL SMALL
	include func.inc
	include	super.inc
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL

	.CODE

func BFNT_EXTEND_HEADER_ANALYSIS
	push	BP
	mov	BP,SP
	push	SI

	; 引数
	handle	= (RETSIZE+1+DATASIZE)*2
	header	= (RETSIZE+1)*2

	CLD
	_push	DS
	_lds	BX,[BP+header]
	mov	CX,[BX].extSize
	mov	SI,[BX].hdrSize
	_pop	DS

	mov	AX,0
	jcxz	short FALL

	push	CX
	_call	SMEM_WGET		; メモリ確保
	jc	short FALL

	push	DS
	mov	DS,AX			;segment
	xor	DX,DX
	mov	BX,[BP+handle]
	mov	AH,3fh			; read handle
	int	21h
	mov	AX,InvalidData
	jc	short return

	mov	DX,SI
	xor	SI,SI
	EVEN

next_header:
	lodsb
	cmp	AL,10h			;透明色ID
	jz	short clear
	add	SI,DX
	sub	SI,3
	lodsw
	or	AX,AX
	jz	short noclear
	sub	CX,DX
	mov	DX,AX
	ja	short next_header
noclear:
	xor	AX,AX			;拡張ヘッダ無い時は透明色:0
	jmp	short return
	EVEN
clear:	lodsw
	lodsb
	and	AX,000fh
	EVEN
return:
	mov	BX,DS
	pop	DS
	push	BX
	_call	SMEM_RELEASE
FALL:
	pop	SI
	pop	BP
	ret	(1+DATASIZE)*2
endfunc

END
