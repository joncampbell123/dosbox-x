; master library - 
;
; Description:
;	mem_assign系で確保されたメモリを管理から除外する
;	このときに、mem_assign_dos/mem_assign_allで確保されていたのなら
;	同時に DOS メモリを解放する。
;
; Function/Procedures:
;	int mem_unassign(void) ;
;	function mem_unassign: Boolean ;
;
; Parameters:
;	none
;
; Returns:
;	1(true)	success( 開放に成功、あるいはもともと割り当てられていない )
;	0(false) failure(hmem_*やsmem_*で確保されたままのものがある)
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
; Assembly Language Note:
;	AX,ES 破壊
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 9/16 Initial: memunasi.asm/master.lib 0.21

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN mem_TopSeg:WORD
	EXTRN mem_OutSeg:WORD
	EXTRN mem_TopHeap:WORD
	EXTRN mem_FirstHole:WORD
	EXTRN mem_EndMark:WORD
	EXTRN mem_MyOwn:WORD

	.CODE

func MEM_UNASSIGN	; mem_unassign() {
	cmp	mem_TopSeg,0
	je	short SUCCESS

	mov	AX,mem_OutSeg	; some heap type memory allocated
	cmp	mem_TopHeap,AX
	jne	short FAILURE

	mov	AX,mem_TopSeg
	cmp	mem_EndMark,AX	; some stack type memory allocated
	jne	short FAILURE

	mov	ES,AX		; for free

	xor	AX,AX
	cmp	mem_MyOwn,AX	; myown が 0 なら開放せず成功
	mov	mem_TopSeg,AX
	je	short SUCCESS

	mov	AH,49h		; free block
	int	21h

SUCCESS:
	mov	AX,1
	ret
FAILURE:
	xor	AX,AX
	stc
	ret
endfunc			; }

END
