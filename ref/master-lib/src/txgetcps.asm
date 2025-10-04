; master library - PC-9801
;
; Description:
;	テキスト画面のカーソル位置取得
;
; Function/Procedures:
;	long text_getcurpos( void ) ;
;
; Parameters:
;	none
;
; Returns:
;	long	HIWORD(DX) = y座標( 0 = 上端 )
;		LOWORD(AX) = x座標( 0 = 左端 )
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
;	NEC MS-DOSのワークエリアを直接参照しています。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/20 Initial

	.MODEL SMALL
	include func.inc

	.CODE

func TEXT_GETCURPOS
	xor	AX,AX
	mov	ES,AX
	mov	DX,AX
	mov	AL,ES:[071Ch]
	mov	DL,ES:[0710h]
	ret
endfunc

END
