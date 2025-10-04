; master library module
;
; Description:
;	�e�L�X�g��ʂɑ��g��`�悷��
;
; Functions/Procedures:
;	void text_frame(int x1, int y1, int x_len, int y_len, unsigned wattr, unsigned iattr, int round ); 
;
; Parameters:
;	x1,y1	����̍��W
;	x2,y2	�E���̍��W
;	wattr	���g�̑���
;	iattr	���̑���
;	round	0=�p�������g, 1=�ۂ��g
;
; Returns:
;	none
;
; Binding Target:
;	Microsoft-C / Turbo-C / Turbo Pascal
;
; Running Target:
;	PC-9801 Normal/Hires.
;
; Requiring Resources:
;	CPU: 8086
;
; Notes:
;	���g��`�悵�܂��B
;	���g���̂� wattr�̑����ŁA�܂ǂ̒���iattr�̑����œh��܂��B
;	���̒��g�͋󔒂œh��ׂ��܂��B
;	���̗��e�Ɋ����̔������������Ă����ꍇ�A���̊������󔒂�
;	�h��ׂ��܂��B
;
;	���̊֐������s����ƁA�Z�~�O���t/�c�����[�h�͏c���ɕύX����܂��B
;	(�m�[�}�����[�h�̏ꍇ�̂�)
;
;	�N���b�s���O��l�͈͂̌����͈�؂����Ȃ��Ă��܂���B
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	�̂�
;	����(���ˏ��F)
;
; Revision History:
;	<from t_box.asm by �̂�>
;	93/ 4/18 Initial: txframe.asm/master.lib 0.16
;

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN TextVramSeg : WORD

	.CODE


func	TEXT_FRAME		; text_frame() {
	push	BP
	mov	BP,SP

	; ����
	x1	= (RETSIZE+7)*2
	y1	= (RETSIZE+6)*2
	x2	= (RETSIZE+5)*2
	y2	= (RETSIZE+4)*2
	fattr	= (RETSIZE+3)*2
	iattr	= (RETSIZE+2)*2
	round = (RETSIZE+1)*2

	push	SI
	push	DI
	push	DS

	mov	AL,0	; �c�����[�h
	out	68h,AL

	mov	BH,[BP+x2]
	inc	BH
	mov	BL,[BP+y2]
	dec	BL

	mov	AL,[BP+y1]
	sub	BL,AL
	mov	CL,80
	mul	CL
	mov	DI,[BP+x1]
	mov	CX,DI
	sub	BH,CL
	add	DI,AX
	shl	DI,1
	mov	DX,[BP+fattr]
	and	DL,not 10h

	test	byte ptr [BP+round],1
	mov	AX,98h
	je	short	KAKU
	mov	AX,9ch
KAKU:
	mov	[BP+round],AX

	mov	AX,TextVramSeg
	mov	DS,AX
	mov	ES,AX

	call	JOGE			; +-------------+ (��)
	add	word ptr [BP+round],2
	add	DI,160

					; |             | (��)
	push	BX
	or	DL,10h
	mov	CH,0
	mov	CL,BL
	mov	SI,CX
	mov	BL,BH
	mov	BH,0
	dec	BX
	shl	BX,1

NAKA_LOOP:
	mov	AX,[DI]
	or	AL,AH	;
	neg	AH	; if ( AH  &&  (AH | AL) & 0x80 )
	sbb	AH,AH
	and	AH,AL
	test	AH,80h
	mov	AX,20h	; ��
	je	short NAKA_RIGHT
	mov	[DI-2],AX

NAKA_RIGHT:
	mov	[DI+2000h],DX
	mov	[DI+BX+2000h],DX

	mov	CX,BX
	shr	CX,1
	inc	CX
	rep	stosw	; �󔒂ł��߂�
	mov	CX,BX
	shr	CX,1
	dec	CX
	sub	DI,BX
	add	DI,2000h
	mov	AX,[BP+iattr]
	rep	stosw
	sub	DI,2000h

	mov	AX,[DI+2]
	or	AL,AH	;
	neg	AH	; if ( AH  &&  (AH | AL) & 0x80 )
	sbb	AH,AH
	and	AH,AL
	test	AH,80h
	je	short NAKA_END
	mov	word ptr [DI+2],20h	; ��
NAKA_END:
	sub	DI,BX

	add	DI,160
	dec	SI
	jnz	short NAKA_LOOP

	and	DL,not 10h
	pop	BX

	call	JOGE			; +-------------+ (��)

	pop	DS
	pop	DI
	pop	SI

	pop	BP
	ret	7*2
endfunc		; }

; DI : ���[
; BH : ����
; DX : ����
; breaks:
;	AX = atr
;	SI = DI
;	CX = 0
JOGE proc near		; ����[��(��)
	mov	AX,[DI]
	or	AL,AH	;
	neg	AH	; if ( AH  &&  (AH | AL) & 0x80 )
	sbb	AH,AH
	and	AH,AL
	test	AH,80h
	je	short EX_RIGHT
	mov	AX,20h	; ��
	mov	[DI-2],AX

EX_RIGHT:
	mov	SI,DI
	mov	AH,0
	mov	AL,BH
	shl	AX,1
	add	DI,AX
	mov	AX,[DI]
	or	AL,AH	;
	neg	AH	; if ( AH  &&  (AH | AL) & 0x80 )
	sbb	AH,AH
	and	AH,AL
	test	AH,80h
	mov	AX,20h	; ��

	je	short EX_END
	mov	[DI],AX
EX_END:
	mov	DI,SI

	; ��ɑ�����h��
	mov	AX,DX
	mov	CH,0
	mov	CL,BH
	add	DI,2000h
	rep	stosw

	sub	DI,2002h
	STD

	mov	SI,[BP+round]
	lea	AX,[SI+1]
	stosw
	mov	AX,0095h
	mov	CH,0
	mov	CL,BH
	dec	CX
	dec	CX
	rep	stosw

	mov	[DI],SI
	CLD

	ret
JOGE endp

END
