; master library - PC98V
;
; Description:
;	グラフィック画面(表裏)を EMSに退避，復元する
;
; Function/Procedures:
;	int graph_backup(int pagemap) ;	(退避)
;	int graph_restore(void) ;	(復元)
;
; Parameters:
;	int pagemap	(pagemap & 1) != 0 最初のページを退避
;			(pagemap & 2) != 0 ２番目の頁を退避
;
; Returns:
;	int	1 = 成功
;		0 = 失敗( EMSが存在しない、足りないなど )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRAPHICS ACCERALATOR: GRCG
;
; Notes:
;	GRCGの各レジスタの内容が破壊されます。
;	アクセスページは、page 0になります。
;	EMSドライバは、Melware, NEC, I･O DATAで動作確認しています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	原作: Mikio( strvram.c )
;	変更・制作(asm): 恋塚昭彦
;
; Revision History:
;	92/12/02 Initial
;	92/12/05 graph_restore() bugfix
;	92/12/28 graph_backup()に引数(頁指定)を追加
;	93/ 1/26 large版 bugfix
;	95/ 3/20 [M0.22k] BUGFIX GRCGをOFFにしていなかった

	.186
	.MODEL SMALL
	include func.inc
	EXTRN	EMS_EXIST:CALLMODEL
	EXTRN	EMS_ALLOCATE:CALLMODEL
	EXTRN	EMS_FREE:CALLMODEL
	EXTRN	EMS_MOVEMEMORYREGION:CALLMODEL
	EXTRN	EMS_ENABLEPAGEFRAME:CALLMODEL

	.DATA
vram_handle	dw	0

	.DATA?
page_map	dw	?

	.CODE

EMSSTRUCT struc
region_length		dd ?
source_memory_type	db ?
source_handle		dw ?
source_initial_offset	dw ?
source_initial_seg_page	dw ?
dest_memory_type	db ?
dest_handle		dw ?
dest_initial_offset	dw ?
dest_initial_seg_page	dw ?
EMSSTRUCT ends

GC_MODEREG equ	7ch
GC_TILEREG equ	7eh
GC_RMW	equ	0c0h
GC_TCR	equ	080h
GC_B	equ	1110b
GC_R	equ	1101b
GC_G	equ	1011b
GC_I	equ	0111b

GRAM_APAGE equ 0a6h


MRETURN macro
	pop	DI
	pop	SI
	leave
	ret 2
	EVEN
	endm

func GRAPH_BACKUP	; {
	enter 18,0
	push	SI
	push	DI

	; 引数
	pagemap	= (RETSIZE+1)*2

	; local variables
	trans	= -18

	mov	SI,[BP+pagemap]		; SI = pagemap
	and	SI,3
	jz	short BAK_ERROR
	mov	page_map,SI
	cmp	vram_handle,0
	je	short BAK_SKIP
BAK_ERROR:
	xor	AX,AX
	MRETURN

BAK_SKIP:
	call	EMS_EXIST
	or	AX,AX
	je	short BAK_ERROR

	mov	AX,2	; 128KB
	cmp	SI,3
	jl	short BAK_1PAGE
	shl	AX,1	; 256KB
BAK_1PAGE:
	push	AX
	push	0
	call	EMS_ALLOCATE
	mov	vram_handle,AX

	or	AX,AX
	je	short BAK_ERROR

	mov	word ptr [BP+trans].region_length,8000h
	mov	word ptr [BP+trans].region_length+2,0

	mov	[BP+trans].source_memory_type,0	; from Main Memory

	mov	[BP+trans].source_initial_seg_page,0a800H

	mov	[BP+trans].dest_memory_type,1	; to EMS

	mov	AX,vram_handle
	mov	[BP+trans].dest_handle,AX

	xor	AX,AX
	mov	[BP+trans].source_handle,AX
	mov	[BP+trans].source_initial_offset,AX
	mov	[BP+trans].dest_initial_offset,AX
	mov	[BP+trans].dest_initial_seg_page,AX

	xor	DI,DI

	; ページのるーぷ
BAK_LOOP:
	shr	SI,1
	jnb	short BAK_SKIPPAGE
	mov	AX,DI
	out	GRAM_APAGE,AL

	xor	AX,AX
	out	GC_MODEREG,AL		; GRCG OFF

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AL,GC_TCR or GC_R
	out	GC_MODEREG,AL
	STI
	mov	AL,0ffh
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL
	out	GC_TILEREG,AL

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AX,GC_TCR or GC_G
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2

	CLI
	mov	AX,GC_TCR or GC_I
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	or	AX,AX
	jne	short BAK_FAILURE

	add	[BP+trans].dest_initial_seg_page,2
BAK_SKIPPAGE:
	inc	DI
	or	SI,SI
	jnz	short BAK_LOOP

	push	SI	; 0
	call	EMS_ENABLEPAGEFRAME

	xor	AX,AX
	out	GRAM_APAGE,AL
	out	GC_MODEREG,AL		; GRCG OFF

	inc	AX			; AX = 1
	jmp	short BAK_RET

BAK_FAILURE:
	push	vram_handle		; 途中で失敗していたら開放する
	call	EMS_FREE

	xor	AX,AX
	mov	vram_handle,AX
	out	GC_MODEREG,AL		; GRCG OFF
BAK_RET:
	MRETURN
endfunc	; }


MRETURN macro
	pop	DI
	pop	SI
	leave
	ret
	EVEN
	endm

func GRAPH_RESTORE	; {
	enter 18,0
	push	SI
	push	DI

	; local variables
	trans = -18

	cmp	vram_handle,0
	je	short RES_ERROR
	cmp	page_map,0
	jne	short RES_START
RES_ERROR:
	xor	AX,AX
	MRETURN
RES_START:

	mov	WORD PTR [BP+trans].region_length,8000H
	mov	WORD PTR [BP+trans].region_length+2,0

	mov	[BP+trans].source_memory_type,1

	mov	AX,vram_handle
	mov	[BP+trans].source_handle,AX

	xor	AX,AX
	mov	[BP+trans].dest_memory_type,AL

	mov	[BP+trans].source_initial_offset,AX
	mov	[BP+trans].source_initial_seg_page,AX
	mov	[BP+trans].dest_handle,AX
	mov	[BP+trans].dest_initial_offset,AX

	mov	[BP+trans].dest_initial_seg_page,0a800H

	mov	DI,page_map
	xor	SI,SI

	shr	DI,1
	cmc
	mov	AX,SI
	adc	AX,AX
	out	GRAM_APAGE,AL	; first page

	; ページのるーぷ
RES_LOOP:
	mov	AL,GC_TCR		; GRCG TCRで全部 0 に～
	CLI
	out	GC_MODEREG,AL
	STI
	xor	AX,AX
	mov	DX,GC_TILEREG
	out	DX,AL
	out	DX,AL
	out	DX,AL
	out	DX,AL

	push	DI
	mov	BX,0a800H
	mov	CX,8000H/2
	mov	DI,AX
	mov	ES,BX
	rep	stosw
	pop	DI

	out	GC_MODEREG,AL		; GRCG OFF

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	CLI				; GRCGタイルの全ビット ON
	mov	AL,GC_RMW or GC_R
	out	GC_MODEREG,AL
	STI
	mov	AL,0ffh
	mov	DX,GC_TILEREG
	out	DX,AL
	out	DX,AL
	out	DX,AL
	out	DX,AL

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	CLI
	mov	AL,GC_RMW or GC_G
	out	GC_MODEREG,AL
	STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2

	;CLI
	mov	AL,GC_RMW or GC_I
	out	GC_MODEREG,AL			; GRCG SETMODE
	;STI

	_push	SS
	lea	AX,[BP+trans]
	push	AX
	call	EMS_MOVEMEMORYREGION
	add	SI,AX

	add	[BP+trans].source_initial_seg_page,2
RES_SKIPPAGE:
	mov	AL,1
	out	GRAM_APAGE,AL

	dec	DI
	jns	short RES_LOOP

	xor	AX,AX
	out	GC_MODEREG,AL			; GRCG OFF

	push	AX
	call	EMS_ENABLEPAGEFRAME

	xor	AX,AX
	out	GRAM_APAGE,AL

	push	vram_handle
	call	EMS_FREE

	mov	vram_handle,0

	neg	SI
	sbb	AX,AX
	inc	AX	; 1 = success, 0 = fault
	MRETURN
endfunc	; }

END
