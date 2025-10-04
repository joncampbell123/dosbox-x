; master library - superimpose
;
; Description:
;	パターンの登録(最下位)
;
; Function/Procedures:
;	int super_entry_at( int num, unsigned patsize, unsigned patseg ) ;
;
; Parameters:
;	num	登録先のパターン番号
;	patsize	パターンの大きさ
;	seg	パターンデータの格納された, hmemブロックセグメントアドレス
;		(このアドレスがそのまま登録される。中身はパターンサイズに
;		 従って mask->B->R->G->Iの順に入っていること, または
;		 TINY形式も可)
;
; Returns:
;	InsufficientMemory	(cy=1) super_bufferが確保できない(9216bytes)
;	GeneralFailure		(cy=1) numが 512以上
;	NoError			(cy=0) 成功
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	すでに登録されている場所に登録すると、以前のデータは開放されます。
;
; Assembly Language Note:
;	AX,flagレジスタを破壊します。
;	d flagは必ず 0 になります。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/12/ 7 Initial: superat.asm/master.lib 0.22
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.186
	.MODEL SMALL
	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patdata:WORD		; superpa.asm
	EXTRN	super_patsize:WORD
	EXTRN	super_buffer:WORD
	EXTRN	super_patnum:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

	.CODE
	EXTRN	HMEM_ALLOC:CALLMODEL		; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL


BUFFER_SIZE equ (16+2)*128*4

func SUPER_ENTRY_AT	; super_entry_at() {
	push	BP
	mov	BP,SP
	push	BX

	CLD

	num	= (RETSIZE+3)*2
	patsize	= (RETSIZE+2)*2
	patseg	= (RETSIZE+1)*2

	mov	BX,[BP+num]
	cmp	BX,MAXPAT
	cmc
	mov	AX,GeneralFailure
	jc	short EXIT

	; super_bufferをチェック
	cmp	super_buffer,0
	jne	short DO_ENTRY
	; 完全に開放された状態(初期値)だった場合

	mov	mem_AllocID,MEMID_super
	push	BUFFER_SIZE/16
	_call	HMEM_ALLOC	; 確保する
	mov	super_buffer,AX
	mov	AX,InsufficientMemory
	jc	short EXIT

	push	ES
	push	CX
	push	DI

	push	DS
	pop	ES
	xor	AX,AX
	mov	DI,offset super_patsize	; パターンサイズ配列を初期化
	mov	CX,MAXPAT
	rep	stosw

	pop	DI
	pop	CX
	pop	ES

DO_ENTRY:
	mov	AX,BX
	shl	BX,1

	cmp	AX,super_patnum
	jae	short ADDING		; 最後尾以降なら…

OVERWRITING:				; 登録最後尾より前なら上書き
	cmp	super_patsize[BX],0
	je	short GO

	push	super_patdata[BX]	; すでに何かあるなら開放
	_call	HMEM_FREE
	jmp	short GO

ADDING:
	inc	AX
	mov	super_patnum,AX

GO:
	mov	AX,[BP+patsize]
	mov	super_patsize[BX],AX
	mov	AX,[BP+patseg]
	mov	super_patdata[BX],AX
	xor	AX,AX			; clc, NoError

EXIT:
	pop	BX
	pop	BP
	ret	6
	EVEN
endfunc		; }

END
