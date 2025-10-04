; master library - GRCG - GRAPHIC - PC-9801
;
; Description:
;	�O���t�B�b�N��ʂւ�8x8dot������`�� [�F�w��]
;
; Function/Procedures:
;	void graph_wank_putsa( int x, int y, int step, char * anks, int color ) ;
;
; Parameters:
;	x,y	�`��J�n������W
;	step	1�������Ƃɐi�߂�h�b�g��
;	anks	���p������
;	color	�F(0�`15)
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
;	����
;
; Revision History:
;	93/ 8/23 Initial: grpwapsa.asm/master.lib 0.21
;	93/ 8/25 [M0.21] far������Ή�


	.186
	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN	wfont_AnkSeg:WORD

	.CODE

func GRAPH_WANK_PUTSA	; graph_wank_putsa() {
	push	BP
	mov	BP,SP
	push	SI
	push	DI

	; ����
	x	= (RETSIZE+4+DATASIZE)*2
	y	= (RETSIZE+3+DATASIZE)*2
	step	= (RETSIZE+2+DATASIZE)*2
	anks	= (RETSIZE+2)*2
	color	= (RETSIZE+1)*2

	_push	DS

	mov	DX,[BP+color]

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

	mov	DX,wfont_AnkSeg

	_lds	SI,[BP+anks]

	; �ŏ��̕����͉�����
	lodsb
	or	AL,AL
	jz	short RETURN	; �����񂪋�Ȃ�Ȃɂ����Ȃ���

	mov	CX,[BP+x]
	mov	DI,[BP+y]
	mov	BX,[BP+step]
	mov	BP,BX

	mov	BX,DI		;-+
	shl	BX,2		; |
	add	DI,BX		; |DI=y*80
	shl	DI,4		;-+
	mov	BX,CX
	and	CX,7		;CL=x%8(shift dot counter)
	shr	BX,3		;AX=x/8
	add	DI,BX		;GVRAM offset address

	mov	BX,0a800h
	mov	ES,BX
	EVEN
LOOPTOP:			; ������̃��[�v
	push	DS
	mov	CH,8
	xor	AH,AH
	shl	AX,3
	mov	BX,AX
	mov	DS,DX		; ank seg

	EVEN

ANK_LOOP:			; �����̕`��
	mov	AL,[BX]
	xor	AH,AH
	ror	AX,CL
	stosw
	add	DI,78
	inc	BX
	dec	CH
	jnz	short ANK_LOOP

	sub	DI,80 * 8
	add	CX,BP		;CH == 0!!
	mov	AX,CX
	and	CX,7
	shr	AX,3
	add	DI,AX

	pop	DS

	lodsb
	or	AL,AL
	jnz	short LOOPTOP

RETURN:
	xor	AL,AL
	out	7ch,AL		;grcg stop

	_pop	DS
	pop	DI
	pop	SI
	pop	BP
	ret	(4+DATASIZE)*2
endfunc			; }

END
