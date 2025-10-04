; master library - MS-DOS
;
; Description:
;	高速メモリ取得(64K以内)
;
; Functions:
;	unsigned smem_wget( unsigned bytesize ) ;
;
; Returns:
;	smem_wget
;		InsufficientMemory(cy = 1) メモリ不足
;		segment           (cy = 0) 確保したメモリ
;
; Notes:
;	レジスタはAXのみを破壊します。
;	まだメモリが割り当てられていない場合、mem_assign_all()を実行します。
;	smem_wget(0)として呼び出すと、現在の末尾位置が得られます。
;
; Revision History:
;	93/ 3/20 Initial
;	93/ 9/14 [M0.21] 冒頭の検査を mem_TopHeap -> mem_TopSeg に修正
;	95/ 3/23 [M0.22k] BUGFIX assign確保されてないときに…
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_EndMark:WORD

	.CODE
	EXTRN MEM_ASSIGN_ALL:CALLMODEL

retfunc ASSIGNALL
	call	MEM_ASSIGN_ALL
	jc	short W_ERROR2
	; 下に続くよ
endfunc
	; unsigned smem_wget( unsigned bytesize ) ;
func SMEM_WGET
	cmp	mem_TopSeg,0	; house keeping
	je	short ASSIGNALL

	push	BX
	mov	BX,SP

	; 引数
	byte_size = (RETSIZE+1)*2

	mov	BX,SS:[BX+byte_size]
	add	BX,15		;切り上げ
	rcr	BX,1		;paragraph size
	shr	BX,1
	shr	BX,1
	shr	BX,1

	mov	AX,mem_EndMark
	add	BX,AX
	jc	short W_ERROR	; more safety...

	cmp	mem_TopHeap,BX
	jc	short W_ERROR

	mov	mem_EndMark,BX
	pop	BX
	ret	2
	EVEN

W_ERROR:
	pop	BX
W_ERROR2:
	mov	AX,InsufficientMemory
	ret	2
endfunc

END
