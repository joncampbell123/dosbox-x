; master library - PC98
;
; Description:
;	外字データをメインメモリに退避，復元する
;
; Function/Procedures:
;	int gaiji_backup( void ) ;		(退避)
;	int gaiji_restore( void ) ;		(復元)
;
;
; Parameters:
;	none
;
; Returns:
;	int	1 = 成功
;		0 = 失敗( 退避･復元回数不一致, メインメモリが足りないなど )
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	既に退避してあるのにもう一度退避しようとするとエラーになります。
;	同じ様に、開放も連続２回実行はできません。
;	退避、開放と交互ならば何回でも実行できます。
;
;	テキスト画面が表示されていると画面の漢字がちらつきます。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 2/ 4 Initial
;	93/ 3/23 メモリマネージャ使用

	.MODEL SMALL
	include func.inc
	EXTRN	HMEM_ALLOC:CALLMODEL
	EXTRN	HMEM_FREE:CALLMODEL

GAIJISIZE equ 256*2*16

	.DATA
backup_mseg	dw	0	; Main Memory Segment

	.CODE

	EXTRN	GAIJI_READ_ALL:CALLMODEL
	EXTRN	GAIJI_WRITE_ALL:CALLMODEL

func GAIJI_BACKUP	; {
	xor	AX,AX
	cmp	backup_mseg,AX
	jne	short B_END		; すでに退避してるならエラー

	mov	AX,GAIJISIZE/16
	push	AX
	call	HMEM_ALLOC
	or	AX,AX
	jz	short B_END

	mov	backup_mseg,AX
	push	AX
	xor	AX,AX
	push	AX
	call	GAIJI_READ_ALL
	mov	AX,1
B_END:
	ret
endfunc			; }

func GAIJI_RESTORE	; {
	mov	AX,backup_mseg
	test	AX,AX
	jz	short R_FAULT

	push	AX		; このpushのみ、HMEM_FREEの引数だよん
	push	AX
	xor	AX,AX
	mov	backup_mseg,AX
	push	AX
	call	GAIJI_WRITE_ALL
	call	HMEM_FREE
	mov	AX,1
R_FAULT:
	ret
endfunc		; }

END
