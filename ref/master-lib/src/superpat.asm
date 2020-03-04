; superimpose & master library module
;
; Description:
;	パターンの登録
;
; Functions/Procedures:
;	int super_entry_pat( int patsize, void far *image_addr, int clear_color ) ;
;
; Parameters:
;	patsize
;	image_addr	パターンの先頭アドレス
;	clear_color	透明色
;
; Returns:
;	InsufficientMemory	(cy=1)	メモリが足りない
;	GeneralFailure		(cy=1)	登録数が多すぎる
;	0～			(cy=0)	成功。登録したパターン番号
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	Heapからメモリをパターンを保持するために取得します。
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
;$Id: superpat.asm 0.09 93/02/19 20:11:35 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/12/ 6 [M0.22] BUGFIX パターンを全部削除したあとだと
;			super_bufferを再確保していた
;	95/ 2/14 [M0.22k] mem_AllocID対応
;	95/ 3/14 [M0.22k] BUGFIX 失敗時、GeneralFailure等が返っていなかった
;	95/ 3/16 [M0.22k] BUGFIX 失敗時、メモリを開放していなかった

	.186
	.MODEL SMALL

	include func.inc
	include super.inc

	.DATA
	EXTRN	super_patnum:WORD		; superpa.asm
	EXTRN	mem_AllocID:WORD		; mem.asm

	.CODE

	EXTRN	SUPER_ENTRY_AT:CALLMODEL	; superat.asm
	EXTRN	HMEM_ALLOCBYTE:CALLMODEL	; memheap.asm
	EXTRN	HMEM_FREE:CALLMODEL		; memheap.asm

MRETURN	macro
	pop	DI
	pop	SI
	pop	BP
	ret	8
	EVEN
	endm

retfunc ENTRY_AT_ERROR	; エラー	in: ES = pattern segment
	push	AX
	push	ES
	call	HMEM_FREE
	pop	AX
	jmp	short ERROR_EXIT
endfunc

retfunc NO_MEMORY	; メモリ不足
	mov	AX,InsufficientMemory

ERROR_EXIT label near	; エラー終了	in: AX = error code
	stc
	MRETURN
endfunc

func SUPER_ENTRY_PAT	; super_entry_pat() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	patsize		= (RETSIZE+4)*2
	image_addr	= (RETSIZE+2)*2
	clear_color	= (RETSIZE+1)*2

	mov	DI,super_patnum
	shl	DI,1			;integer size
	mov	AX,[BP+patsize]
	mov	DX,AX
	mul	AH
	mov	BX,AX	; BX = plane size
	shl	AX,2
	add	AX,BX
	mov	mem_AllocID,MEMID_super
	push	AX
	_call	HMEM_ALLOCBYTE		; allocate (plane size * 5) bytes
	jc	short NO_MEMORY

	mov	ES,AX			; ES = パターン領域

	push	super_patnum	; 登録番号
	push	DX		; patsize
	push	AX		; パターン領域
	_call	SUPER_ENTRY_AT		; BX,ESは破壊されないこと
	jc	short ENTRY_AT_ERROR

	push	DS

	; パターンデータを確保パターン領域に転送する

	lds	SI,[BP+image_addr]
	mov	DI,BX
	mov	CX,BX
	shl	CX,1			;4plane / word
	rep	movsw
	push	ES
	pop	DS			; DS もパターン領域に

	; マスクプレーンを生成する

	mov	SI,BX			; plane size (パターン先頭)
	mov	DX,[BP+clear_color]
	mov	DH,DL

	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	jc	short BLUE_REV
	rep	movsb
	jmp	short RED
	EVEN
BLUE_REV:
	lodsb
	not	AL
	stosb
	loop	short BLUE_REV
	EVEN
RED:
	; red
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_RED:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_RED

	; green
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_GREEN:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_GREEN

	; inten
	mov	CX,BX
	xor	DI,DI
	shr	DH,1
	sbb	AH,AH
	EVEN
OR_INTEN:
	lodsb
	xor	AL,AH
	or	[DI],AL
	inc	DI
	loop	short OR_INTEN

	; パターン内の透明部分をマスクで繰り抜く

	test	DL,DL			; 透明色が 0 なら省略
	jz	short return

	mov	AH,4
	EVEN
CLEAR:
	xor	SI,SI
	mov	CX,BX
	EVEN
CLEAR_PLANE:
	lodsb
	and	[DI],AL
	inc	DI
	loop	short CLEAR_PLANE
	dec	AH
	jnz	short CLEAR
	EVEN

	; 終わり
return:
	pop	DS
	mov	AX,super_patnum
	dec	AX
	MRETURN
endfunc			; }

END
