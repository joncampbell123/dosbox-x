; master library - PC98V
;
; Description:
;	Palettes,PaletteTone�̓��e�����ۂ̃n�[�h�E�F�A�ɏ������ށB
;
; Function/Procedures:
;	void palette_show( void ) ;
;
; Parameters:
;	none
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
;	�EPaletteTone�� 0��菬������� 0 �ɁA200���傫�����200�Ɋۂ߂܂��B
;	�EPaletteTone �� 0 �` 100�̂Ƃ��́A 0 = ���A100 = �{���̐F�ƂȂ�A
;	  �풓�p���b�g�݊��ƂȂ�܂��B
;	�EPaletteTone �� 101 �` 200�̂Ƃ��́A200 = ���ƁA�풓�p���b�g��
;	�@�قȂ�A�t���b�V���ɂȂ�܂��B
;	�EPaletteNote�� 0 �ȊO�̂Ƃ��́A�t�� 8�K���p�̕\�����s���܂��B
;	�E�t���␳�́ANEC�� 98NOTE�n��8�K���\���@��p�̃p���b�g�␳��
;	�@�s�����̂ł��B���̂Ƃ��A���]��Ԃɂ�����炸�A�����ł����邭
;	�@�Ȃ�悤�ɕ␳���܂��B
;
;	����m�F�@��(�m�F��): ArtsLink la femy�Ńe�X�g
;	  98NS(hila), 98NS/E(����), 98NS/T(M.Kit), 98NL(mau), 98NA(guri)
;	  98NS/R(��), 98NS/L(�m�n�u�`)
;	  98NC(����) 9821Ne(����)���J���[�@��͉t���␳�ΏۊO
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	92/11/16 Initial
;	92/11/26 tone : 101�`200�ɑΉ�, 200=��
;	93/ 4/ 6 LCD�Ή�
;	93/ 5/22 �t���̔��]��Ԏ�������
;	93/12/ 5 [M0.22] Palettes[] 0..15 -> 0..255
;	94/ 1/ 9 [M0.22] NEC�@�ł̔��]��Ԏ����Ή���NS/T�ȍ~�ɂ��Ή�

	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN PaletteTone : WORD
	EXTRN Palettes : WORD
	EXTRN PaletteNote : WORD

	.CODE

func PALETTE_SHOW	; palette_show() {
	CLD
	push	SI
	mov	AX,PaletteTone
	cwd		; if AX < 0 then AX = 0
	not	DX	;
	and	AX,DX	;
	sub	AX,200	; if AX >= 200 then AX = 200
	sbb	DX,DX	;
	and	AX,DX	;
	add	AX,200	;
	mov	DH,AL		; DH = tone

	xor	BX,BX

	mov	CH,BL		; 0
	cmp	DH,100
	jna	short SKIP
	mov	CH,0fh
	sub	DH,200		; AL = 200 - AL
	neg	DH
SKIP:
	mov	SI,offset Palettes
	mov	DL,100

	cmp	PaletteNote,BX	; 0
	jne	short	LCD


PLOOP:	mov	AL,BL
	out	0a8h,AL	; palette number

	lodsw
	shr	AX,4
	mov	CL,AH
	and	AL,0fh
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	out	0ach,AL	; r
	mov	AL,CL
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	out	0aah,AL	; g
	lodsb
	shr	AL,4
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	out	0aeh,AL	; b

	inc	BX
	cmp	BX,16
	jl	short PLOOP

	pop	SI
	ret

	; �t���`�`�`
	EVEN
LCD:
	mov	BX,DX
IF 0
	mov	DX,0ae8eh
	in	AL,DX
	and	AL,4
	cmp	AL,1
ELSE
	mov	DX,0871eh
	mov	AL,0a0h
	out	0f6h,AL
	in	AL,DX
	cmp	AL,0ffh
	jnz	short NEWNOTE
	mov	DX,0ae8eh
	in	AL,DX
	shr	AL,2
NEWNOTE:
	shr	AL,1
	cmc
ENDIF
	sbb	AL,AL
	mov	CS:XORVAL,AL	; ���]
	mov	DX,BX

	push	DI	;  1byte
	mov	DI,0	; +3byte
			; =4(even)
LLOOP:
	mov	AX,DI
	out	0a8h,AL	; palette number

	lodsw
	mov	BX,AX	; BL = r  BH = g
	shr	BX,4
	and	BL,CH
	lodsb		; AL = b
	and	AL,CH
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	xchg	AL,BH	; BH = b'  AL = g
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	xchg	AL,BL	; BL = g'  AL = r
	xor	AL,CH
	mul	DH
	div	DL
	xor	AL,CH
	xchg	AL,BL	; BL = r'  AL = g
	mov	AH,BH	; AL = g'  AH = BH = b'

	cmp	BH,AL	; BL = r   AL = g   AH = BH = b
	ja	short S1
	mov	BH,AL
S1:	cmp	BH,BL
	ja	short S2
	mov	BH,BL	; BH = max(r,g,b)
S2:
	shl	AL,1
	add	AL,BL
	shl	AL,1
	add	AL,AH
	add	AL,BH
	mov	CL,3		; al = al * 9 * 2 / ((1+2+4+1)*15) ;
	mul	CL		; -> �񕪂��� al = al * 3 / 20
	mov	CL,20
	div	CL
	shr	AL,1		; al = al / 2 + (al & 1) ;
	adc	AL,0
	sub	AL,2		; al = max(al-2,0)
	cmc
	sbb	AH,AH
	and	AH,AL
	xor	AH,0
	org $-1
XORVAL	db ?

	shr	AH,1
	sbb	AL,AL
	and	AL,15
	out	0aeh,AL	; b
	shr	AH,1
	sbb	AL,AL
	and	AL,15
	out	0ach,AL	; r
	shr	AH,1
	sbb	AL,AL
	and	AL,15
	out	0aah,AL	; g
	inc	DI
	cmp	DI,16
	jl	short LLOOP

	pop	DI
	pop	SI
	ret
endfunc			; }

END
