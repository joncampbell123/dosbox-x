; master library - PC98
;
; Description:
;	テキスト画面を EMSまたはメインメモリに退避，復元する
;
; Function/Procedures:
;	int text_backup( int use_main ) ;	(退避)
;	int text_restore( void ) ;		(復元)
;
; Parameters:
;	int use_main	0 = EMSのみ, 1 = EMSで失敗したらメインメモリ
;
; Returns:
;	int	1 = 成功
;		0 = 失敗( EMSが存在しない、EMS,メインメモリが足りないなど )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal / Hireso
;
; Requiring Resources:
;	CPU: i8086
;
; Notes:
;	既に退避してあるのにもう一度退避しようとするとエラーになります。
;	同じ様に、開放も連続２回実行はできません。
;	退避、開放と交互ならば何回でも実行できます。
;	30行BIOSのpush,popを待避,復元の際に呼び出しています。
;
;	画面の行数は保存／復元していません。
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
;	92/12/05 bugfix
;	92/12/28 text_restoreのVRAM内容復元前に各種パラメータを復元するように
;	93/ 3/23 メモリマネージャ使用
;	93/ 8/25 [M0.21] 30biosのpush/popを実行するようにした。
;	93/12/11 [M0.22] 30bios popを呼ぶタイミング修正。
;	95/ 2/14 [M0.22k] mem_AllocID対応

	.MODEL SMALL
	include func.inc
	include super.inc
	EXTRN	EMS_EXIST:CALLMODEL
	EXTRN	EMS_ALLOCATE:CALLMODEL
	EXTRN	EMS_FREE:CALLMODEL
	EXTRN	EMS_READ:CALLMODEL
	EXTRN	EMS_WRITE:CALLMODEL

	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL


	.DATA?
	EXTRN TextVramSeg:WORD
	EXTRN mem_AllocID:WORD		; mem.asm

backup_curx	dw	?
backup_cury	dw	?
backup_sysline	dw	?

	.DATA
backup_handle_seg dw	0	; EMS Handle / Main Memory Segment
backup_type	  db	0	; 0 = バックアップされてない, 1=MAIN, 2=EMS

BACKUP_TYPE_MAIN	equ 1
BACKUP_TYPE_EMS		equ 2

	.CODE

	EXTRN	TEXT_SYSTEMLINE_SHOWN:CALLMODEL
	EXTRN	TEXT_GETCURPOS:CALLMODEL
	EXTRN	TEXT_LOCATE:CALLMODEL
	EXTRN	TEXT_SYSTEMLINE_SHOW:CALLMODEL
	EXTRN	TEXT_SYSTEMLINE_HIDE:CALLMODEL
	EXTRN	TEXT_CURSOR_SHOW:CALLMODEL

	EXTRN	BIOS30_PUSH:CALLMODEL
	EXTRN	BIOS30_POP:CALLMODEL


TVRAMSIZE equ 4000h

func TEXT_BACKUP
	push	BP
	mov	BP,SP
	mov	BP,[BP+(RETSIZE+1)*2]	; use_main
	neg	BP
	sbb	BP,BP		; if use_main != 0 then BP=ffff else BP=0

	xor	AX,AX
	cmp	backup_type,AL
	jne	B_NG		; すでに退避してるならエラー

	_call	EMS_EXIST
	dec	AX
	js	short B_TRY_MAIN

	xor	AX,AX
	push	AX
	mov	AX,TVRAMSIZE
	push	AX
	_call	EMS_ALLOCATE
	test	AX,AX
	jz	short B_TRY_MAIN

	mov	backup_handle_seg,AX

	push	AX	; handle
	xor	AX,AX
	push	AX	; write offset
	push	AX	;
	push	TextVramSeg	; text seg
	push	AX		; text offset
	push	AX			; size
	mov	AX,TVRAMSIZE		;
	push	AX			;
	_call	EMS_WRITE
	dec	AX
	mov	AL,BACKUP_TYPE_EMS
	js	short B_OK

	push	backup_handle_seg
	_call	EMS_FREE

B_TRY_MAIN:
	xor	AX,AX
	or	BP,BP
	jz	short B_NG
	mov	AX,TVRAMSIZE/16
	push	AX
	mov	mem_AllocID,MEMID_textback
	_call	HMEM_ALLOC
	jc	short B_NG

	mov	backup_handle_seg,AX
	mov	ES,AX
	push	DS
	push	SI
	push	DI
	mov	DS,TextVramSeg
	xor	DI,DI
	mov	SI,DI
	mov	CX,TVRAMSIZE/2
	rep	movsw
	pop	DI
	pop	SI
	pop	DS
	mov	AL,BACKUP_TYPE_MAIN

B_OK:
	mov	backup_type,AL
	_call	TEXT_SYSTEMLINE_SHOWN
	mov	backup_sysline,AX
	_call	TEXT_GETCURPOS
	mov	backup_curx,AX
	mov	backup_cury,DX
	_call	BIOS30_PUSH
	mov	AX,1
B_NG:
	pop	BP
	ret	2
endfunc

func TEXT_RESTORE
	cmp	backup_type,1
	jc	short R_FAULT

	_call	BIOS30_POP

	cmp	backup_sysline,0
	jz	short R_SOFF
	_call	TEXT_SYSTEMLINE_SHOW
	jmp	short R_START
R_SOFF:
	_call	TEXT_SYSTEMLINE_HIDE

R_START:
	_call	TEXT_CURSOR_SHOW
	push	backup_curx
	push	backup_cury
	_call	TEXT_LOCATE

	mov	BX,TextVramSeg
	mov	AX,backup_handle_seg

	cmp	backup_type,BACKUP_TYPE_EMS
	je	short R_EMS

R_MAIN:
	push	AX		; このAXのみ、HMEM_FREEの引数だよん
	push	DS
	push	SI
	push	DI
	mov	DS,AX
	mov	ES,BX
	xor	SI,SI
	mov	DI,SI
	mov	CX,TVRAMSIZE/2
	rep	movsw
	pop	DI
	pop	SI
	pop	DS
	_call	HMEM_FREE

	jmp	short R_END

R_FAULT:
	xor	AX,AX
	ret

R_EMS:
	push	AX		; push handle

	xor	AX,AX
	push	AX		; push 0L
	push	AX		; 

	push	BX		; push TextVramSeg:0
	push	AX		;

	push	AX		; 
	mov	AX,TVRAMSIZE	; push (long)TVRAMSIZE
	push	AX		; 
	_call	EMS_READ
	or	AX,AX
	jnz	short R_FAULT
	push	backup_handle_seg
	_call	EMS_FREE
R_END:
	xor	AX,AX
	mov	backup_type,AL
	inc	AX			; AX = 1
	ret
endfunc

END
