; master library
;
; Description:
;	ディレクトリエントリの移動
;
; Functions:
;	int dos_move( const char far * source, const char far * dest ) ;
;
; Running Target:
;	MS-DOS v?
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 4/20 Initial: dosmove.asm/master.lib 0.16
;	93/10/28 [M0.21] bugfix: DSを保存してなかった

	.MODEL SMALL
	include func.inc
	.CODE

func DOS_MOVE	; {
	push	BP
	mov	BP,SP
	; 引数
	source = (RETSIZE+3)*2
	dest   = (RETSIZE+1)*2

	push	DS
	push	DI

	mov	AH,56h
	lds	DX,[BP+source]
	les	DI,[BP+dest]
	int	21h
	sbb	AX,AX
	inc	AX

	pop	DI
	pop	DS
	pop	BP
	ret	8
endfunc	; }

END
