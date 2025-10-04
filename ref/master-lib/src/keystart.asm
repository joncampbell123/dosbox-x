; master library
; Description:
;	一般的なキー入力サポート
;	(NEC DOS拡張ファンクション使用)
;
; Functions:
;	void key_start( void ) ;
;	void key_end( void ) ;
;
;	unsigned key_scan( void ) ;
;	unsigned key_wait( void ) ;
;		0～0ffh: normal keys
;		100h:	help key
;		101h～10ah: F1～F10
;		10bh～114h: SHIFT+F1～F10
;		120h～124h: VF1～VF5
;		125h～129h: SHIFT+VF1～VF5
;		12ah～138h: CTRL+F1～VF5
;
; Running Target:
;	PC-9801 series (Normal and Hires.)
;
; Revision History:
;	93/ 3/16 Initial
;	93/ 5/11 [M0.16] F･11～F･15対応
;	93/ 9/11 [M0.21] CTRL+F･?対応
;	94/ 6/23 [M0.23] 多重起動防止にフラグ設置

	.MODEL SMALL
	include func.inc

	doskey_top equ 21
	doskey_end equ 32
	fkey_end equ 38h

	HELPKEY equ 100h

	.DATA
key_started	dw	0

	.DATA?

key_backup	db	786 dup (?)
keywork		db	10 dup (?)

	.DATA
	EXTRN key_table_normal:WORD
	EXTRN key_table_shift:WORD
	EXTRN key_table_ctrl:WORD
	EXTRN key_table_alt:WORD
	EXTRN key_back_buffer:WORD


	.CODE
;
func KEY_START		; {
	push	SI

	; CTRL+ファンクションキーをソフトキー化
	mov	CL,0fh
	mov	AX,0
	int	0dch

	test	key_started,1
	jne	short KEY_RESTART
	mov	key_started,3

	; ファンクションキーの内容を保存する
	mov	CX,0ch
	mov	AX,00ffh
	mov	DX,offset key_backup
	int	0dch

KEY_RESTART:
	; ファンクションキー定義
	mov	keywork,0feh
	mov	word ptr keywork+1,2020h	; 全部空白
	mov	word ptr keywork+3,2020h
	mov	word ptr keywork+5,7f20h
	mov	BX,1
FKEY_LOOP:
	mov	WORD PTR keywork+7,BX
	mov	CX,0dh
	mov	AX,BX
	mov	DX,offset keywork
	int	0dch
	inc	BX
	cmp	BX,20
	jle	short FKEY_LOOP

	; 特殊キー定義
	mov	keywork,07fh
	mov	BX,doskey_top
DKEY_LOOP:
	mov	WORD PTR keywork+1,BX
	mov	CX,0dh
	mov	AX,BX
	mov	DX,offset keywork
	int	0dch
	inc	BX
	cmp	BX,fkey_end
	jle	short DKEY_LOOP

	pop	SI
	ret
endfunc			; }

func KEY_END		; {
	test	key_started,2
	jnz	short KEY_END_START	; foolproof
	ret
KEY_END_START:
	and	key_started,not 1

	mov	CX,0dh
	mov	AX,00ffh
	mov	DX,offset key_backup
	int	0dch

	; CTRL+ファンクションキーのソフトキー化を解除
	mov	CL,0fh
	mov	AX,1
	int	0dch

	ret
endfunc			; }

func KEY_SCAN		; {
	xor	AX,AX
	xchg	AX,key_back_buffer
	test	AX,AX
	jnz	short SCAN_RET

	mov	AH,6
	mov	DL,0FFh
	int	21h
	mov	AH,0
	jz	short SCAN_NOKEY

	cmp	AL,07fh
	jne	short SCAN_RET

key_convert label CALLMODEL
	mov	AH,7
	int	21h

	mov	AH,1
	cmp	AL,doskey_top
	jb	short SCAN_RET	; function keys: 101h～114h
	cmp	AL,doskey_end
	jae	short SCAN_RET	; shift + function keys: 120h~129h

	sub	AL,doskey_top
	mov	DX,0
	mov	ES,DX
	mov	AH,DL
	mov	BX,AX
	shl	BX,1

	mov	AL,ES:53ah
	and	AL,19h		; 10h:CTRL	08h:GRPH	01h:SHIFT
	jnz	short L00
	mov	AX,key_table_normal[BX]
	ret

L00:	cmp	AL,01h
	jne	short L01
	mov	AX,key_table_shift[BX]
	ret

L01:	cmp	AL,08h
	jne	short L02
	mov	AX,key_table_alt[BX]
	ret

L02:	cmp	AL,10h
	jne	short L03
	mov	AX,key_table_ctrl[BX]
	ret

L03:	mov	AX,HELPKEY	; 無効入力はすべて HELPKEY
SCAN_RET:
	ret

SCAN_NOKEY:
	mov	AX,-1
	ret
endfunc			; }

func KEY_WAIT
	xor	AX,AX
	xchg	AX,key_back_buffer
	test	AX,AX
	jnz	short KEY_READ

	mov	AH,7
	int	21h
	mov	AH,0
	cmp	AL,07fh
	je	short KW_CONV
	ret
KW_CONV:
	call	key_convert
	cmp	AX,-1
	je	short KEY_WAIT
KEY_READ:
	ret
endfunc

END
