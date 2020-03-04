; superimpose & master library module
;
; Description:
;	キャラクタの登録
;	全キャラクタの開放
;
; Functions/Procedures:
;	int super_entry_char( int patnum ) ;
;	void super_free_char(void) ;
;
; Parameters:
;	patnum	新規にキャラクタに割り当てるパターン番号
;
; Returns:
;	int	GeneralFailure		文字数がMAXCHARを超えた
;		InsufficientMemory	メモリ不足
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
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
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: superchr.asm 0.03 93/01/19 17:09:07 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/ 9/16 [M0.21] charnum -> super_charcount
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 4/ 7 [M0.22k] super_free_char()追加, super_charfree対応
;

	.MODEL SMALL
	include func.inc
	include super.inc

	EXTRN HMEM_ALLOC:CALLMODEL	; memheap.asm
	EXTRN HMEM_FREE:CALLMODEL	; memheap.asm

	.DATA
	EXTRN super_patsize:WORD	; superpa.asm
	EXTRN super_charfree:WORD	; superpa.asm

	EXTRN super_chardata:WORD	; superch.asm
	EXTRN super_charnum:WORD	; superch.asm
	EXTRN super_charcount:WORD	; superch.asm
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE


func SUPER_ENTRY_CHAR	; super_entry_char() {
	; 
	patnum = (RETSIZE+0)*2

	mov	CX,super_charcount
	cmp	CX,MAXCHAR
	mov	AX,GeneralFailure
	cmc
	jb	short FALL

	mov	BX,SP
	mov	BX,SS:[BX+patnum]
	shl	BX,1		;near pointer
	mov	AX,super_patsize[BX]
	mov	BL,AH
	xor	BH,BH		;CX = x_size
	mov	AH,BH		;AH = 0
	inc	BX		;++ and word size
	inc	BX
	and	BX,0fffeh
	mul	BX
	;shl	AX,1		;4 plane
	;shl	AX,1
	add	AX,3
	shr	AX,1
	shr	AX,1
	push	AX
	mov	mem_AllocID,MEMID_super
	call	HMEM_ALLOC
	jc	short MEMERR

	shl	CX,1		;near pointer
	mov	BX,CX
	mov	super_chardata[BX],AX

	mov	BX,SP
	mov	AX,SS:[BX+patnum]

	mov	BX,CX
	mov	super_charnum[BX],AX
	mov	AX,super_charcount
	inc	super_charcount
	mov	super_charfree,offset SUPER_FREE_CHAR
FALL:
	ret	2
MEMERR:
        mov     AX,InsufficientMemory
	ret	2
endfunc			; }


func SUPER_FREE_CHAR	; super_free_char() {
	xor	DX,DX			;0
	mov	BX,super_charcount
	dec	BX
	js	short can_done		; 何もなかったら終わり

	shl	BX,1			;near pointer

can_loop:
	mov	AX,super_chardata[BX]
	or	AX,AX
	jz	short can_skip
	push	AX
	call	HMEM_FREE
can_skip:
	mov	super_chardata[BX],DX	;0
	mov	super_charnum[BX],DX	;0
	dec	BX
	dec	BX
	jns	short can_loop
can_done:
	mov	super_charcount,DX	;0
	mov	super_charfree,DX	;0
	ret
endfunc			; }

END
