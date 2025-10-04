; Master Library - PC98V - V30
;
; Description:
;	特定の点のカラーコードをテーブルによって変換する
;
; Functions:
;	void graph_xlat_dot( int x, int y, char trans[16] ) ;
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	none
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Returns:
;	pset( x,y, trans[ point(x,y) ] ) ってなかんじです。
;
; Author:
;	恋塚昭彦
;
; Revision History:
;	92/11/08 Initial
;	92/11/22 master.libにちゃんと追加(はじめからMaster..って書いてあるし)
;	93/ 3/12 bugfix
;	93/ 3/23 for tiny model
;

BLUE_PLANE	equ 0a800h
RED_PLANE	equ 0b000h	; unuse
GREEN_PLANE	equ 0b800h
INTENSITY_PLANE	equ 0e000h


	.MODEL SMALL
	include func.inc

	.CODE
	.186

MRETURN macro
	pop	BP
	ret	(2+DATASIZE)*2
	EVEN
	endm

retfunc	return
	MRETURN
endfunc

func GRAPH_XLAT_DOT
	push	BP
	mov	BP,SP

	; 引数
	x	= (RETSIZE+1+DATASIZE+1)*2
	y	= (RETSIZE+1+DATASIZE)*2
	trans	= (RETSIZE+1)*2

	mov	AX,SS:[BP+x]
	cmp	AX,640		; X boundary
	jae	short return

	mov	DX,SS:[BP+y]
	cmp	DX,400		; Y boundary
	jae	short return

	push	SI
	push	DI
	push	DS

	mov	DI,DX	; DI = y * 80
	shl	DI,2
	add	DI,DX
	shl	DI,4

	;		; AX = x

	mov	CX,AX	;
	shr	AX,3	;
	add	DI,AX	;
	and	CL,7	;
	mov	AX,8000h;	AL = 0
	shr	AX,CL	;	AH = bit

	; 読み込み
	mov	DX,BLUE_PLANE
	mov	DS,DX
	mov	CL,[DI]
	mov	CH,[DI+8000h]
	mov	DH,GREEN_PLANE shr 8
	mov	DS,DX
	mov	DL,[DI]
	mov	SI,INTENSITY_PLANE
	mov	DS,SI
	mov	DH,[DI]

	; 色コード作成
	test	CL,AH
	jz	short SKIP1
	xor	CL,AH
	inc	AX
SKIP1:
	test	CH,AH
	jz	short SKIP2
	xor	CH,AH
	or	AL,2
SKIP2:
	test	DL,AH
	jz	short SKIP3
	xor	DL,AH
	or	AL,4
SKIP3:
	test	DH,AH
	jz	short SKIP4
	xor	DH,AH
	or	AL,8
SKIP4:

	; 変換
IF DATASIZE EQ 1
	mov	BX,seg DGROUP
	mov	DS,BX
ENDIF
	_lds	BX,[BP+trans]
	xlatb

	; 書き込み
	shr	AL,1
	sbb	BL,BL
	shr	AL,1
	sbb	BH,BH

	and	BL,AH
	and	BH,AH
	or	CX,BX
	mov	BX,BLUE_PLANE
	mov	DS,BX
	mov	[DI],CL
	mov	[DI+8000h],CH

	shr	AL,1
	sbb	BL,BL
	shr	AL,1
	sbb	BH,BH

	and	BL,AH
	and	BH,AH
	or	DX,BX
	mov	BX,GREEN_PLANE
	mov	DS,BX
	mov	[DI],DL
	mov	BH,INTENSITY_PLANE shr 8
	mov	DS,BX
	mov	[DI],DH

	pop	DS
	pop	DI
	pop	SI
	MRETURN
endfunc

END
