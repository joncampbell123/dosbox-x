; master library - MS-DOS
;
; Description:
;	指定サイズのDOSのメモリを確保し、メモリマネージャへ割り当てる
;
; Functions/Procedures:
;	int mem_assign_dos( unsigned parasize ) ;
;
; Noters:
;	レジスタは AXのみ破壊します。
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/23 Initial (0.14)
;	93/ 3/23 bugfix (0.15)

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	mem_MyOwn:WORD

	.CODE
	EXTRN	MEM_ASSIGN:CALLMODEL

	; int mem_assign_dos( unsigned parasize ) ;
	; 破壊: AXのみ
	; メモリ不足なら cy=1
func MEM_ASSIGN_DOS
	push	BX
	mov	BX,SP
	; 引数
	parasize = (RETSIZE+1)*2

	mov	BX,SS:[BX+parasize]
	mov	AH,48h		; 確保する
	int	21h
	jc	short NOMEM
	push	AX
	push	BX
	call	MEM_ASSIGN
	xor	AX,AX
	mov	mem_MyOwn,1
NOMEM:
	neg	AX
	pop	BX
	ret	2
endfunc

END
