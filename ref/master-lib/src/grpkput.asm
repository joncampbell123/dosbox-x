; superimpose & master library module
;
; Description:
;	�O���t�B�b�N��ʂɊ�����1�����\������
;
; Functions/Procedures:
;	void graph_kanji_put( int x, int y, char * kanji, int color ) ;
;
; Parameters:
;	x,y	�`�捶����W
;	kanji	�����������\���|�C���^�B���̐擪��1������`�悷��B
;	color	�F(0�`15)
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
;	Kazumi(���c  �m)
;	����(���ˏ��F)
;
; Revision History:
;
;$Id: kanjiput.asm 0.02 93/01/14 23:46:35 Kazumi Rel $
;
;	93/ 3/20 Initial: master.lib <- super.lib 0.22b
;	93/11/22 [M0.21] bugfix, return size��2�{���ĂȂ������B����Ⴀ
;

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD

	.CODE

func GRAPH_KANJI_PUT
	push	BP
	mov	BP,SP
	push	DI

	x	= (RETSIZE+3+DATASIZE)*2
	y	= (RETSIZE+2+DATASIZE)*2
	kanji	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	DX,[BP+color]
	_push	DS
	_lds	BP,[BP+kanji]
	mov	BP,DS:[BP]
	_pop	DS

	; GRCG setting..
	pushf
	mov	AL,0c0h		;RMW mode
	CLI
	out	7ch,AL
	popf
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL
	shr	DX,1
	sbb	AL,AL
	out	7eh,AL

	; CG dot access
	mov	AL,0bh
	out	68h,AL

	mov	AX,DI		;-+
	shl	AX,2		; |
	add	DI,AX		; |DI=y*80
	shl	DI,4		;-+
	mov	AX,CX
	and	CX,7h		;CL=x%8(shift dot counter)
	shr	AX,3		;AX=x/8
	add	DI,AX		;GVRAM offset address
	mov	ES,graph_VramSeg

	mov	AX,BP
	xchg	AH,AL
	shl	AH,1
	cmp	AL,9fh
	jnb	short SKIP
		cmp	AL,80h
		adc	AX,0fedfh	; (stc)	; -2,-(20h + 1)
SKIP:	sbb	AL,-2
	and	AX,7f7fh
	out	0a1h,AL
	mov	AL,AH
	out	0a3h,AL
	mov	DX,16
	xor	CH,CH

	EVEN
PUT_LOOP:
	mov	AL,CH
	or	AL,00100000b	;L/R
	out	0a5h,AL
	in	AL,0a9h
	mov	AH,AL
	mov	AL,CH
	out	0a5h,AL
	in	AL,0a9h
	mov	BH,AL
	mov	BL,0
	shr	AX,CL
	shr	BX,CL
	xchg	AL,AH
	stosw
	mov	ES:[DI],BL
	add	DI,78		;next line
	inc	CH
	dec	DX
	jnz	short PUT_LOOP

	; CG code access
	mov	AL,0ah
	out	68h,AL

	; GRCG off
	xor	AL,AL
	out	7ch,AL		;grcg stop

	pop	DI
	pop	BP
	ret	(3+DATASIZE)*2
endfunc

END
