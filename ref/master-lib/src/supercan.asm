; superimpose & master library module
;
; Description:
;	登録されているパターンを削除する
;
; Functions/Procedures:
;	int super_cancel_pat( int num ) ;
;
; Parameters:
;	num	削除するパターン番号
;
; Returns:
;	NoError:	(cy=0) 成功
;	GeneralFailure:	(cy=1) その番号は登録されていない
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
;$Id: supercan.asm 0.02 93/01/15 11:45:11 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN super_patsize:WORD	; superpa.asm
	EXTRN super_patdata:WORD	; superpa.asm
	EXTRN super_patnum:WORD		; superpa.asm

	.CODE
	EXTRN	HMEM_FREE:CALLMODEL	; memheap.asm

func SUPER_CANCEL_PAT	; super_cancel_pat() {
	mov	BX,SP

	num	= (RETSIZE+0)*2

	xor	DX,DX			;0
	mov	BX,SS:[BX+num]
	cmp	BX,super_patnum
	jae	short error		; 最後のパターンより後ならエラー

	mov	CX,BX
	shl	BX,1			;near pointer
	mov	AX,super_patsize[BX]
	or	AX,AX
	jz	short error		; そもそも確保されてないならエラー
	push	super_patdata[BX]
	call	HMEM_FREE
	mov	super_patdata[BX],DX	;0
	mov	super_patsize[BX],DX	;0
	inc	CX
	cmp	CX,super_patnum
	jne	short skip
	; 最後パターンを消したので、残ったうちの最後のパターンを検索する
canceled:
	dec	super_patnum
	jz	short skip
	dec	BX
	dec	BX
	mov	CX,super_patdata[BX]
	jcxz	short canceled
	EVEN
skip:
	mov	AX,NoError
	clc
	ret	2
	EVEN
error:
	stc
	mov	AX,GeneralFailure
	ret	2
endfunc			; }

END
