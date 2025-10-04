; master library - PC-9801
;
; Description:
;	�W���C�X�e�B�b�N�֘A�̏����ݒ�
;
; Function/Procedures:
;	int js_start( int force ) ;
;
; Parameters:
;	force	0 = SAJ-98���Ȃ���΃T�E���h�{�[�h����������
;		1 = SAJ-98���Ȃ���΃T�E���h�{�[�h������Ƃ݂Ȃ�
;		2 = SAJ-98�������肵�Ȃ�
;
; Returns:
;	0 = �W���C�X�e�B�b�N�͑S���g�p���Ȃ�
;	1 = �T�E���h�{�[�h��F������
;	2 = SAJ-98��F������(�T�E���h�{�[�h�͖���)
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
; Assembly Language Note:
;	
;
; Compiler/Assembler:
;	TASM 3.0
;	OPTASM 1.6
;
; Author:
;	���ˏ��F
;
; Revision History:
;	93/ 5/ 2 Initial:jsstart.asm/master.lib 0.16
;	93/ 5/10 SAJ-98�Ή��ł������Ȃ�?
;	93/ 6/22 [M0.19] SAJ-98�D��ɕύX

	.MODEL SMALL
	include func.inc

	.DATA
	EXTRN js_bexist:WORD
	EXTRN js_saj_port:WORD

	.CODE
	EXTRN SOUND_I:NEAR
	EXTRN SOUND_O:NEAR

func JS_START	; {
	; ����
	force = (RETSIZE+0)*2

	; SAJ-98����

	mov	CX,3
	mov	DX,0e3e0h
SAJLOOP:
	mov	BX,40h
SAJ1:
	mov	AX,BX
	out	DX,AL
	in	AL,DX
	shl	AL,1
	rcr	AH,1
	inc	DX
	inc	DX
	in	AL,DX
	dec	DX
	dec	DX
	shl	AL,1
	rcr	AH,1
	cmp	AH,BL
	jnz	short	NEXT
	shl	BL,1
	jnc	short SAJ1

	; found
	mov	js_saj_port,DX
	mov	js_bexist,0
	mov	AX,2
	ret	2

NEXT:
	add	DH,2
	loop	short SAJLOOP
	xor	AX,AX
	mov	js_saj_port,AX


	; �T�E���h�{�[�h����

	mov	CX,256

	mov	DX,188h
	mov	BX,SP
	cmp	word ptr SS:[BX+force],1
	je	short ARU
	ja	short NAI
LO1:
	in	AL,DX
	inc	AL
	jnz	short ARU
	loop	short LO1
NAI:
	xor	AX,AX	; �Ȃ�
	jmp	short OWARI
ARU:
	pushf
	CLI
	mov	BH,07h
	call	SOUND_I
	and	AL,3fh
	or	AL,80h	; reg 7 �̏��2bit�� 10 �ɂ���
	mov	BL,AL
	call	SOUND_O
	popf

	mov	AX,1

OWARI:
	mov	js_bexist,AX
	ret	2
endfunc		; }

END
