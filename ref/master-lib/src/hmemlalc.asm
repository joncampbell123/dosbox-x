; master library
;
; Description:
;	ヒープメモリ取得 long版
;
; Functions:
;	unsigned hmem_lallocate( unsigned long bytesize ) ;
;
; Returns:
;	unsigned 	 (cy=0) 確保したセグメント
;			0(cy=1) 管理メモリ不足
;
; Notes:
;	AX以外の全てのレジスタを保存します。
;	hmem_lallocate(0)として呼び出すと、メモリ不足を返します。
;
; Running Target:
;	MS-DOS
;
; Author:
;	恋塚昭彦
;
; Rebision History:
;	93/ 3/31 Initial
;
	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE

	EXTRN	HMEM_ALLOC:CALLMODEL

func HMEM_LALLOCATE	; hmem_lallocate() {
	push	BX
	mov	BX,SP
	;
	byteh = (RETSIZE+2)*2
	bytel = (RETSIZE+1)*2
	mov	AX,SS:[BX+byteh]
	mov	BX,SS:[BX+bytel]
	add	BX,15
	adc	AX,0

	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	jnz	short NOMEM
	rcr	BX,1

	push	BX
	call	HMEM_ALLOC
	pop	BX
	ret	4

NOMEM:
	mov	AX,0
	stc
	pop	BX
	ret	4
endfunc			; }

END
