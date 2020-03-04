; master library - MS-DOS
;
; Description:
;	深いディレクトリを作成する
;
; Function/Procedures:
;	int dos_makedir( const char * path ) ;
;
; Parameters:
;	path	パス名(最後に :,\または/が付いていると失敗します)
;
; Returns:
;	0	失敗(ドライブが無効、おかしな文字があるなど)
;	1	成功
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
;	失敗した場合、途中までディレクトリが作成されている場合があります。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/12/14 Initial

	.MODEL SMALL
	include func.inc

	EXTRN	FILE_BASENAME:CALLMODEL

	.CODE

; 実際の作成処理
; IN: DS:DI = char * const path
; IO: SI=result( 1で初期化すること )
makedir	PROC NEAR

	_push	DS
	push	DI
	call	FILE_BASENAME	; BX = file_basename(path) ;
	mov	BX,AX
	xor	AX,AX

	cmp	[BX],AL		; ドライブ指示 or rootなら失敗
	stc
	je	short MAKE_RESULT

	mov	DX,DI
	mov	AH,39h		; makedir
	int	21h		; 作ってみてできたら成功
	jnb	short MAKE_RET
	cmp	AX,5
	je	short MAKE_RET; 存在するかrootに空きがなければ成功!!(ひ)

	xor	AX,AX
	push	[BX-1]		; 駄目なら一つ上を作ってみる
	mov	[BX-1],AL	; 0
	push	BX
	call	makedir
	pop	BX
	pop	AX
	mov	[BX-1],AL

	test	SI,SI
	jz	short MAKE_RET

	mov	AH,39h		; makedir
	mov	DX,DI
	int	21h		; 一つ上ができたらもう一度作ってみる
MAKE_RESULT:
	sbb	SI,SI
	inc	SI

MAKE_RET:
	ret
	EVEN
makedir	ENDP


func DOS_MAKEDIR
	mov	BX,SP
	push	DI
	_push	DS

	_lds	DI,SS:[BX+RETSIZE*2]	; DS:DI = path

	xor	AX,AX

	cmp	BYTE PTR [DI+1],':'
	jne	short NODRIVE

	mov	AL,[DI]
	and	AL,01fh		; make drive number( 0 = '@' )

NODRIVE:
	mov	DL,AL
	mov	AH,36h
	int	21h		; ドライブの有効性検査のために残り容量を…
	inc	AX
	jz	short EXIT

	push	SI
	mov	SI,1
	call	makedir
	mov	AX,SI
	pop	SI

EXIT:
	_pop	DS
	pop	DI
	ret	DATASIZE*2
endfunc

END
