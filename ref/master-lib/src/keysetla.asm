; master library
;
; Description:
;	ファンクションキーのラベルを設定する
;	(NEC DOS拡張ファンクション使用)
;
; Functions:
;	void key_set_label( int num, const char * label ) ;
;		num =   1～10: F･1～10
;		    =  11～20: SHIFT+F･1～10
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	93/ 3/16 Initial

	.MODEL SMALL
	include func.inc
	.CODE

func KEY_SET_LABEL	; {
	push	BP
	mov	BP,SP
	sub	SP,10
	num	= (RETSIZE+1+DATASIZE)*2
	lab	= (RETSIZE+1)*2

	keyset	= -10

	mov	CX,[BP+num]
	mov	CH,0
	cmp	CX,1
	jl	short	KSET_ERROR
	cmp	CX,20
	jg	short	KSET_ERROR

	mov	byte ptr [BP+keyset],0feh

	push	DS
	pop	ES
	_les	BX,[BP+lab]
	mov	AX,ES:[BX]
	mov	[BP+keyset+1],AX
	mov	AX,ES:[BX+2]
	mov	[BP+keyset+3],AX
	mov	AL,ES:[BX+4]

	mov	AH,7fh
	mov	[BP+keyset+5],AX
	mov	AX,CX
	mov	[BP+keyset+7],AX
	mov	CX,0dh
	push	DS
	push	SS
	pop	DS
	lea	DX,[BP+keyset]
	int	0dch
	pop	DS

KSET_ERROR:
	mov	SP,BP
	pop	BP
	ret	(1+DATASIZE)*2
endfunc			; }

END
