; master library - DOS - COPY
;
; Description:
;	ファイルのコピー
;
; Function/Procedures:
;	int dos_copy( int src_fd, int dest_fd, unsigned long copy_len ) ;
;
; Parameters:
;	src_fd    コピー元ファイルハンドル
;	dest_fd   コピー先ファイルハンドル
;	copy_len  コピーするバイト数( 0ffffffffhならばEOFまで )
;
; Returns:
;	NoError			成功
;
;	GeneralFailure		読み込み失敗(長さがcopy_lenに満たない)
;	GeneralFailure		書き込み失敗(ディスク不足?)
;
;	InsufficientMemory	作業メモリ不足
;	AccessDenied		どちらかのファイルハンドルのアクセス権限が無効
;	InvalidHandle		どちらかのファイルハンドルが無効
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	MS-DOS 2.1 or later
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	　copy_len = 0ffffffffh の場合のみ、途中で srd_fd が EOF に達した場合
;	でも NoError が返ります。
;
; Assembly Language Note:
;	NoErrorのときは cy=0, それ以外は cy=1になっています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 8/ 1 Initial: doscopy.asm/master.lib 0.20
;	93/ 8/26 [M0.21] BUGFIX メモリの解放ができてなかった(^^;
;	93/ 9/14 [M0.21] バッファサイズを32Kから順に 2Kまで半減させて挑戦
;	93/11/17 [M0.21] うう(;_;)単純なミスだあ。サイズ指定したら失敗してた

	.MODEL SMALL
	include func.inc
	include super.inc

	.CODE
	EXTRN	SMEM_WGET:CALLMODEL
	EXTRN	SMEM_RELEASE:CALLMODEL
BUFSIZE equ 32768
MIN_BUFSIZE equ 2048

func DOS_COPY	; dos_copy() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; 引数
	src_fd	= (RETSIZE+4)*2
	dest_fd	= (RETSIZE+3)*2
	copy_len = (RETSIZE+1)*2

	mov	CX,BUFSIZE
ALLOCLOOP:
	push	CX
	_call	SMEM_WGET
	jnc	short GO

	shr	CX,1
	cmp	CX,MIN_BUFSIZE
	jnc	short ALLOCLOOP
	jmp	short MEM_ERROR
GO:

	push	DS
	mov	DS,AX

	mov	DI,[BP+copy_len+2]
	mov	SI,[BP+copy_len]
L:
	test	DI,DI
	jnz	short GO_MAX
	cmp	CX,SI
	jbe	short GO_MAX
	mov	CX,SI
GO_MAX:
	xor	DX,DX
	mov	BX,[BP+src_fd]
	mov	AH,3fh		; read handle
	int	21h
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX
	jc	short ERROR
	cmp	AX,CX
	jz	short CONT

	mov	CX,AX
	mov	AX,[BP+copy_len]
	and	AX,[BP+copy_len+2]
	inc	AX
	mov	AX,GeneralFailure
	stc
	jnz	short ERROR

	xor	DI,DI
	mov	SI,CX

CONT:
	xor	DX,DX
	mov	BX,[BP+dest_fd]
	mov	AH,40h		; write handle
	int	21h
	sbb	DX,DX
	xor	AX,DX
	sub	AX,DX
	jc	short ERROR

	cmp	AX,CX
	mov	AX,GeneralFailure
	stc
	jne	short ERROR

	sub	SI,CX
	sbb	DI,0
	jnz	short L
	cmp	SI,DI		; 0
	jne	short L

	mov	AX,SI		; 0 (= NoError)

ERROR:
	mov	BX,DS
	pop	DS
	push	BX
	_call	SMEM_RELEASE
MEM_ERROR:
	pop	DI
	pop	SI
	pop	BP
	ret	8
endfunc		; }

END
