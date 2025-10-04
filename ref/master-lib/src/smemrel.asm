; master library - MS-DOS
;
; Description:
;	高速メモリ取得で得たメモリの開放
;
; Functions:
;	void smem_release( unsigned memseg ) ;
;
; Parameters:
;	memseg	開放する[smem_wget,smem_lgetのどちらかで得た]セグメントアドレス
;
; Returns:
;	none
;
; Notes:
;	レジスタは全て保存します。
;
; Revision History:
;	93/ 3/20 Initial
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN mem_EndMark:WORD

	.CODE

	; void smem_release( unsigned memseg ) ;
func SMEM_RELEASE
	push	BX
	mov	BX,SP
	; 引数
	memseg	= (RETSIZE+1)*2
	mov	BX,SS:[BX+memseg]
	mov	mem_EndMark,BX
	pop	BX
	ret	2
endfunc

END
