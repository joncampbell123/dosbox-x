; master library - (pf.lib)
;
; Description:
;	オートマチックモード。int 21hにフックする/解除する
;
; Functions/Procedures:
;	void pfstart(const char *parfile);
;	void pfend(void);
;
; Parameters:
;	parfile		parアーカイブファイル名
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS
;
; Requiring Resources:
;	CPU: 186
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	iR
;	恋塚昭彦
;
; Revision History:
; PFINT21.ASM       6,253 94-09-17   23:22
;	95/ 1/10 Initial: pfint21.asm/master.lib 0.23

	.186
	.model SMALL
	include func.inc
	include pf.inc

;		global	C _fstrncpy	: proc
;;		global	C printf	: proc				;DBG


	.DATA?

farptr		struc
	ofst	dw	?
	sgmt	dw	?
farptr		ends

parfilename	db	128 dup(?)
;filename	db	128 dup(?)
pf		dw	?
handle		dw	?

	.DATA

;;	string	db	0ah,'%05d AX:%04x BX:%04x CX:%04x DX:%04x',0	;DBG

	.CODE
	EXTRN	PFOPEN:CALLMODEL
	EXTRN	PFCLOSE:CALLMODEL
	EXTRN	PFREAD:CALLMODEL
	EXTRN	PFREWIND:CALLMODEL
	EXTRN	PFSEEK:CALLMODEL

;;trapc		dw	0						;DBG
org_int21	farptr	<0,0>
on_hook		db	0

func PFSTART	; pfstart() {
	push	BP
	mov	BP,SP

	;arg	parfile:dataptr
	parfile = (RETSIZE+1)*2

	CLD
	; すでに start してないかチェック
	mov	AX,CS:org_int21.ofst
	or	AX,CS:org_int21.sgmt
	jnz	short PFSTART_copy

	; DOSファンクションリクエストのエントリを退避
	msdos	GetInterruptVector,21h
	mov	CS:org_int21.ofst,BX
	mov	CS:org_int21.sgmt,ES

	; 変数の初期化
	mov	pf,0
	mov	handle,-1

	; 新しいファンクションリクエストエントリをセット
	push	DS
	push	CS
	pop	DS
	mov	DX,offset pfint21
	msdos	SetInterruptVector,21h
	pop	DS

PFSTART_copy:
	;strcpy( parfilename, parfile )
	push	SI
	push	DI
	_push	DS

	mov	CX,-1	; CX = strlen(parfile)+1
	mov	AL,0
	s_push	DS
	s_pop	ES
	_les	DI,[BP+parfile]
	repne	scasb
	not	CX
	sub	DI,CX

	mov	SI,DI	; copy
	mov	DI,offset parfilename
	_push	DS
	_push	ES
	_pop	DS
	_pop	ES
	shr	CX,1
	rep	movsw
	adc	CX,CX
	rep	movsb

	_pop	DS
	pop	DI
	pop	SI

	pop	BP
	ret	(DATASIZE)*2
endfunc	; }


func PFEND	; pfend() {
	mov	AX,CS:org_int21.ofst
	or	AX,CS:org_int21.sgmt
	jz	short PFEND_return

	; ファンクションリクエストエントリを元に戻す
	push	DS
	lds	DX,CS:org_int21
	msdos	SetInterruptVector,21h
	pop	DS
	xor	AX,AX
	mov	CS:org_int21.ofst,AX
	mov	CS:org_int21.sgmt,AX

	; オープン中のハンドルがあればクローズ
	cmp	pf,AX
	je	short PFEND_return

	push	pf
	_call	PFCLOSE

PFEND_return:
	ret
endfunc	; }


pfint21	proc	far	; {
;;	inc	trapc						;DBG
	cmp	CS:on_hook,0
	jz	short _Hook
	jmp	dword ptr CS:org_int21

	_Hook:
	pusha		; push	AX CX DX BX SP BP SI DI
	push	DS
	push	ES
	mov	BP,SP
	mov	DI,DGROUP
	mov	DS,DI
	assume	DS:DGROUP

	_FLAGS = + 24
	_CS = +22
	_IP = +20
	_AX = +18
	_CX = +16
	_DX = +14
	_BX = +12
	_SP = +10
	_BP = +8
	_SI = +6
	_DI = +4
	_DS = +2
	_ES = +0

	inc	CS:on_hook
	; 割込みフラグを割込み前に戻す
	push	word ptr [BP+_FLAGS]
	popf

;;		pusha							;DBG
;;		call	printf C,offset DGROUP:string,trapc,AX,BX,CX,DX
;;		popa							;DBG

	; AH(ファンクション)でswitch
	mov	byte ptr CS:default,AH
	mov	SI,offset switch_table - 4
i_switch:
	add	SI,4
	cmp	AH,CS:[SI]
	jne	short i_switch
	mov	DI,handle
	jmp	word ptr CS:[SI + 2]

	switch_table	label	word
	dw	OpenHandle,		_Open
	dw	CloseHandle,		_Close
	dw	ReadHandle,		_Read
	dw	MoveFilePointer,	_MoveFilePointer
	dw	ForceDuplicateFileHandle,_CheckHandle2
	dw	WriteHandle,		_CheckHandle
	dw	DuplicateFileHandle,	_CheckHandle
	dw	EndProcess,		_Terminate
	dw	GetSetDateTimeofFile,	_CheckHandle
	dw	LockUnlock,		_CheckHandle
	dw	IOCTL,			_IOCTL
	default	dw	?,			_Thru	; 番兵


	;-------------------------------------------------------------------
	;	OpenHandle
	;		DS:DX	パス名
	;		AL	ファイルアクセスコントロール
_Open:
	test	AL,0fh
	jnz	_Thru		; リードモードでない

	or	DI,DI
	jns	_Thru		; 既にオープンしている

;		mov	DI,offset DGROUP:filename	; lea	DI,filename
;		call	_fstrncpy C,DI,DS,DX,[BP+_DS],length filename
;		call	PFOPEN Pascal,offset DGROUP:[parfilename],DI

	;call	PFOPEN Pascal,offset DGROUP:[parfilename],DX
	_push	DS
	push	offset parfilename
	push	[BP+_DS]
	push	DX
	_call	PFOPEN
				; この場合 DS==ssを確認したほうがよい

	or	AX,AX
	jz	_Thru		; 無条件にthruはよくないかも

	mov	pf,AX
	mov	ES,AX
	mov	ES,ES:[pf_bf]
	mov	AX,ES:[b_hdl]	; pf->bf->handle
	mov	handle,AX
	jmp	_OK

	;-------------------------------------------------------------------
	;	CloseHandle
	;		BX	ファイルハンドル
_Close:
	cmp	BX,DI		; ハンドルチェック
	jne	_Thru

	push	pf
	_call	PFCLOSE

	mov	pf,0
	mov	handle,-1
	jmp	_OK

	;-------------------------------------------------------------------
	;	ReadHandle
	;		BX	ファイルハンドル
	;		DS:DX	バッファの位置
	;		CX	読み込むべきバイト数
_Read:
	cmp	BX,DI		; ハンドルチェック
	jne	short _Thru

	;call	PFREAD Pascal,[BP+_DS],DX,CX,pf
	push	[BP+_DS]
	push	DX
	push	CX
	push	pf
	_call	PFREAD

	jmp	_OK

	;-------------------------------------------------------------------
	;	MoveFilePointer
	;		BX	ファイルハンドル
	;		CX:DX	移動するバイト数
	;		AL	移動方法 00:先頭 01:カレント 02:最後
_MoveFilePointer:
	cmp	BX,DI		; ハンドルチェック
	jne	short _Thru

	or	CX,CX
	jl	short _Error		; 負のオフセットは不可
	cmp	AL,1
	je	short i_seek
	jl	short i_seek_top

	; ファイルの終りからの移動は、終わりに移動するだけ
	mov	ES,pf
	mov	DX,word ptr ES:[pf_osize]
	mov	CX,word ptr ES:[pf_osize+2]
	sub	DX,word ptr ES:[pf_loc]
	sbb	CX,word ptr ES:[pf_loc+2]
	jmp	short i_seek

i_seek_top:
	push	CX
	push	DX
	push	pf
	_call	PFREWIND
	pop	DX
	pop	CX
i_seek:
	push	pf
	push	CX
	push	DX
	_call	PFSEEK	; 戻り値 DX AX が現在位置

	; 現在位置を返す
	mov	[BP+_DX],DX
	jmp	short _OK

	;-------------------------------------------------------------------
	;	EndProcess
	;
_Terminate:
	; フックを戻してから終了
	lds	DX,CS:org_int21
	msdos	SetInterruptVector,21h
	jmp	short _Thru

	;-------------------------------------------------------------------
	;	IOCTRL
_IOCTL:
	mov	cl,AL
	mov	AX,1
	shl	AX,cl
	test	AX,CS:IOCTL_table
	jnz	short _CheckHandle
	jmp	short _Thru

	IOCTL_table	dw	0001010011001111b
	;	bit
	;	0	Get IOCTL Data
	;	1	Set IOCTL Data
	;	2	Recieve IOCTL Character
	;	3	Send IOCTL Character
	;	6	Get Input IOCTL Status
	;	7	Get Output IOCTL Status
	;	a	IOCTL is Redirected Handle
	;	c	Generic IOCTL (for handle)

	;-------------------------------------------------------------------
	;	ハンドルのチェック
_CheckHandle2:
	mov	CX,DI		; ハンドルチェック
	je	short _Error		; エラーにしないでクローズする
				; 方がよいかも

_CheckHandle:
	cmp	BX,DI		; ハンドルチェック
	je	short _Error

	;-------------------------------------------------------------------
_Thru:
	dec	CS:on_hook
	;	mov	CX,[org_int21.ofst]
	;	mov	AX,[org_int21.sgmt]
	;	xchg	CX,[BP+_CX]
	;	xchg	AX,[BP+_AX]
	;	pop	ES DS DI SI BP BX BX DX
	push	word ptr [BP+_FLAGS]
	popf
	;	retf
	pop	ES
	pop	DS
	popa
	cli
	jmp	dword ptr CS:org_int21

	;-------------------------------------------------------------------
_Error:
	or	byte ptr [BP+_FLAGS],01h	; stc
	mov	AX,1		; 適切なエラーコードを返すべきだが
				; とりあえず固定で1を返す
	jmp	short i_return

	;-------------------------------------------------------------------
_OK:
	and	byte ptr [BP+_FLAGS],0feh	; clc

i_return:
	mov	[BP+_AX],AX
	dec	CS:on_hook
	pop	ES
	pop	DS
	;	assume	DS:NOTHING
	popa		; pop	DI SI BP SP BX DX CX AX
	iret
pfint21	endp		; }

END
