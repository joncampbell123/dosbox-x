; Master Library - Key Sense
;
; Description:
;	現在のキーコードグループ中のキーの押下状態を調べる。
;
; Function/Procedures:
;	int key_sense( int keygroup ) ;
;
; Parameters:
;	char keygroup	キーコードグループ
;
; Returns:
;	キーの押下状態
;
; Binding Target:
;	TASM
;
; Running Target:
;	NEC PC-9801 Normal mode
;
; Requiring Resources:
;	CPU: 8086
;
; Compiler/Assembler:
;	TASM 2.51
;
; Note:
;	キーバッファとは関係なく動作する。
;	キーリピートによってとりこぼす恐れがあるので二度実行し
;	結果のorをとること
;	んーっと、一般人仕様(tab 8のMASM Mode)なので安心してください。
;
; Author:
;	SuCa
;
; Rivision History:
;	93/06/16 initial
;	93/ 6/17 Initial: keysense.asm/master.lib 0.19

	.MODEL SMALL
	include func.inc

	.CODE

func	KEY_SENSE
	keygroup = RETSIZE*2

	mov	bx,sp
	mov	ax,ss:[bx+keygroup]
	mov	ah,04h
	int	18h
	mov	al,ah
	mov	ah,0
	ret	2
endfunc

END
