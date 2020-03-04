; master library - MS-DOS
;
; Description:
;	高速メモリ取得(64K以上)
;
; Functions:
;	unsigned smem_lget( unsigned long bytesize ) ;
;
; Returns:
;	InsufficientMemory(cy = 1) メモリ不足
;	segment           (cy = 0) 確保したメモリ
;
; Notes:
;	レジスタはAXのみを破壊します。
;	まだメモリが割り当てられていない場合、mem_assign_all()を実行します。
;	smem_lget(0)として呼び出すと、現在の末尾位置が得られます。
;
; Revision History:
;	93/ 3/20 Initial
;	93/ 9/14 [M0.21] 冒頭の検査を mem_TopHeap -> mem_TopSeg に修正
;	93/12/15 [M0.22] (x_x)bugfix, 引数の上位ワードがランダムになってた
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
	jc	short L_ERROR2
	; 下に続くよ
endfunc
	; unsigned smem_lget( unsigned long bytesize ) ;
func SMEM_LGET
	cmp	mem_TopSeg,0	; house keeping
	je	short ASSIGNALL

	push	BX
	mov	BX,SP

	; 引数
	byte_size = (RETSIZE+1)*2

	mov	AX,SS:[BX+byte_size+2]
	mov	BX,SS:[BX+byte_size]
	add	BX,15		;切り上げ
	adc	AX,0
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	rcr	BX,1
	shr	AX,1
	jnz	short L_ERROR
	rcr	BX,1

	mov	AX,mem_EndMark
	add	BX,AX
	jc	short L_ERROR

	cmp	mem_TopHeap,BX
	jc	short L_ERROR

	mov	mem_EndMark,BX
	pop	BX
	ret	4
	EVEN

L_ERROR:
	pop	BX
L_ERROR2:
	stc
	mov	AX,InsufficientMemory
	ret	4
endfunc

END
