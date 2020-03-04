; superimpose & master library module
;
; Description:
;	
;
; Functions/Procedures:
;	void slice_put(int x, int y, int num, int line) ;
;
; Parameters:
;	
;
; Returns:
;	
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
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(âúìc  êm)
;	óˆíÀ(óˆíÀè∫ïF)
;
; Revision History:
;
;$Id: sliceput.asm 0.03 92/05/29 20:14:14 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	95/ 6/ 7 [M0.23] BUGFIX lineÇ™â∫à 4bitÇ≈andÇ≥ÇÍÇƒÇΩ
;

	.186
	.MODEL SMALL
	include func.inc
	.DATA
	EXTRN	super_patsize:WORD, super_patdata:WORD

	.CODE

func SLICE_PUT
	push	BP
	mov	BP,SP
	push	DS
	push	SI
	push	DI

	x	= (RETSIZE+4)*2
	y	= (RETSIZE+3)*2
	num	= (RETSIZE+2)*2
	line	= (RETSIZE+1)*2

	mov	DX,[BP+x]
	mov	DI,[BP+y]
	mov	CX,[BP+line]
	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	shr	DX,3		;DX=x/8
	add	DI,DX		;GVRAM offset address
	mov	BX,[BP+num]
	shl	BX,1		;integer size & near pointer
	mov	DX,super_patsize[BX]		;pattern size (1-8)

	push	BX
	push	DX

	mov	AL,DH
	xor	AH,AH
	mov	DH,AH		;DH = 0
	dec	DX
	mul	DX
	mov	BP,AX
	pop	DX
	pop	BX
	mov	DS,super_patdata[BX]		;BX+2 -> BX
	mov	DL,DH
	xor	DH,DH
	mov	SI,BP
	add	SI,DX
	jcxz	short start	; 95/6/7
	EVEN
search_line:
	add	SI,DX
	loop	search_line
start:
	mov	BX,DI
	mov	CX,DX
	mov	AX,0a800h
	mov	ES,AX
	rep	movsb
	add	SI,BP
	mov	DI,BX
	mov	CX,DX
	mov	AX,0b000h
	mov	ES,AX
	rep	movsb
	add	SI,BP
	mov	DI,BX
	mov	CX,DX
	mov	AX,0b800h
	mov	ES,AX
	rep	movsb
	add	SI,BP
	mov	DI,BX
	mov	CX,DX
	mov	AX,0e000h
	mov	ES,AX
	rep	movsb

	pop	DI
	pop	SI
	pop	DS
	pop	BP
	ret	8
endfunc

END
