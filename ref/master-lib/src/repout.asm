; superimpose & master library module
;
; Description:
;	指定位置に表示されている指定キャラクタを消去する[高速版]
;
; Functions/Procedures:
;	void repair_out( int x, int y, int charnum ) ;
;
; Parameters:
;	x,y	左上端
;	charnum	既に表示されているキャラクタ番号
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801
;
; Requiring Resources:
;	CPU: V30
;
; Notes:
;	　横方向が8dot単位で復元されるので、左右が余分に大きく消去されます。
;	　指定パターンの大きさ丁度で消去するには、super_out()を使用して
;	下さい。
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(奥田  仁)
;	恋塚(恋塚昭彦)
;
; Revision History:
;
;$Id: repout.asm 0.02 92/05/29 20:11:56 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD
	EXTRN	super_charnum:WORD, super_chardata:WORD

	.CODE

func REPAIR_OUT
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+3)*2
	y	= (RETSIZE+2)*2
	charnum	= (RETSIZE+1)*2

	mov	DX,[BP+x]
	mov	AX,[BP+y]
	mov	CX,[BP+charnum]

	mov	BP,AX		;-+
	shl	BP,2		; |SI=y*80
	add	BP,AX		; |
	shl	BP,4		;-+
	shr	DX,3		;AX=x/8
	add	BP,DX		;GVRAM offset address

	shl	CX,1		;integer size & near pointer
	mov	BX,CX
	mov	BX,super_charnum[BX]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]	;pattern size (1-8)
	mov	BX,CX
	xor	SI,SI
	mov	DS,super_chardata[BX]	;BX+2 -> BX
	mov	CL,DH
	xor	CH,CH
	inc	CX
	inc	CX		;1 byte
	and	CX,not 1
	mov	BX,80
	sub	BL,CL
	mov	AX,CX
	mov	CX,0a800h
	call	REPAIR
	mov	CX,0b000h
	call	REPAIR
	mov	CX,0b800h
	call	REPAIR
	mov	CX,0e000h
	call	REPAIR

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	6
endfunc


REPAIR	proc	near
	mov	ES,CX
	mov	DI,BP
	mov	DH,DL
	shr	AX,1
	jnb	short REPAIR_EVEN

	EVEN
REPAIR_ODD:
	mov	CX,AX
	movsb
	rep	movsw
	lea	DI,[DI+BX]
	dec	DH
	jnz	short REPAIR_ODD
	rcl	AX,1
	ret

	EVEN
REPAIR_EVEN:
	mov	CX,AX
	rep	movsw
	add	DI,BX
	dec	DH
	jnz	short REPAIR_EVEN
	shl	AX,1
	ret
REPAIR	endp

END
