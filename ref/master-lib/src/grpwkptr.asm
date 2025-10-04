; master library - graphic - wfont - kanji - putc - GRCG - PC98V
;
; Description:
;	�O���t�B�b�N��ʂւ̈��k�S�p�����`��
;
; Function/Procedures:
;	void graph_wkanji_put(int x, int y, const char * str);
;
; Parameters:
;	x,y    ������W
;	str    ������̐擪�A�h���X�B����1�����ڂ��`�悳��܂��B
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801V
;
; Requiring Resources:
;	CPU: V30
;	GRCG
;
; Notes:
;	
;
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	Kazumi(���c  �m)
;
; Revision History:
;	93/ 7/ 2 Initial:grpwkput.asm/master.lib 0.20
;	93/11/22 [M0.21] bugfix, return size��2�{���ĂȂ������B����Ⴀ
;	95/ 1/31 Initial: grpwkptr.asm / master.lib 0.23


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN graph_VramSeg:WORD
	EXTRN wfont_Plane1:BYTE,wfont_Plane2:BYTE
	EXTRN wfont_Reg:BYTE

	.CODE

func GRAPH_WKANJI_PUT_RIGHT	; graph_wkanji_put_right() {
	push	BP
	mov	BP,SP
	push	DI

	x	= (RETSIZE+2+DATASIZE)*2
	y	= (RETSIZE+1+DATASIZE)*2
	kanji	= (RETSIZE+1)*2

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	_push	DS
	_lds	BP,[BP+kanji]
	mov	BP,DS:[BP]
	_pop	DS

	; GRCG setting..
	mov	AL,0c0h		;RMW mode
	out	7ch,AL
	mov	AL,wfont_Reg
	out	7eh,AL
	out	7eh,AL
	out	7eh,AL
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
	mov	DX,8
	xor	CH,CH

	EVEN
PUT_LOOP:
	; GRCG setting..
	mov	AL,wfont_Plane1	;RMW mode
	out	7ch,AL

	mov	AL,CH
;	or	AL,00100000b	;L/R
	out	0a5h,AL
	in	AL,0a9h
	mov	AH,AL
	mov	AL,0
;	mov	AL,CH
;	out	0a5h,AL
;	in	AL,0a9h
;	mov	BH,AL
;	mov	BL,0
	shr	AX,CL
;	shr	BX,CL
	xchg	AL,AH
	mov	ES:[DI],AX	;stosw
;	mov	ES:[DI+2],BL
	inc	CH

	; GRCG setting..
	mov	AL,wfont_Plane2	;RMW mode
	out	7ch,AL

	mov	AL,CH
;	or	AL,00100000b	;L/R
	out	0a5h,AL
	in	AL,0a9h
	mov	AH,AL
	mov	AL,0
;	mov	AL,CH
;	out	0a5h,AL
;	in	AL,0a9h
;	mov	BH,AL
;	mov	BL,0
	shr	AX,CL
;	shr	BX,CL
	xchg	AL,AH
	mov	ES:[DI],AX	;stosw
;	mov	ES:[DI+2],BL
	add	DI,80		;next line
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
	ret	(2+DATASIZE)*2
endfunc				; }

END
