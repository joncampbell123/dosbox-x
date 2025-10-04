; master library - BGM
;
; Description:
;	���ʉ������ۂ�1���炷
;
; Function/Procedures:
;	int _bgm_effect_sound(void);
;
; Parameters:
;
;
; Returns:
;	FINISH	���t����
;	NOTFIN	�܂����t�f�[�^�ɑ���������
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
;	femy(��  ����)		: �I���W�i���EC�����
;	steelman(���  �T�i)	: �A�Z���u�������
;	����
;
; Revision History:
;	93/12/19 Initial: b_e_snd.asm / master.lib 0.22 <- bgmlibs.lib 1.12
;	94/ 2/15 [M0.22a] 38�����̒l�������Ă�div zero���o�Ȃ��悤��(by ����)
;	94/ 4/11 [M0.23] AT�݊��@�Ή�

	.186
	.MODEL SMALL
	include func.inc
	include bgm.inc

	.DATA
	EXTRN	glb:WORD	;SGLB
	EXTRN	esound:WORD	;SESOUND

	EXTRN	Machine_State:WORD

	.CODE

func _BGM_EFFECT_SOUND	; _bgm_effect_sound() {
	;if (*(esound[glb.scnt - 1].sptr) == SEND)
	mov	AX,glb.scnt
	dec	AX
	shl	AX,3			; * (type SESOUND)
	mov	BX,AX
	les	BX,esound[BX].sptr
	mov	CX,ES:[BX]
	mov	BX,AX
	jcxz	short END_OF_SOUND	; cmp	CX,SEND / jne

	;bgm_bell2(*(esound[glb.scnt - 1].sptr)++);
	add	word ptr esound[BX].sptr,2

	test	Machine_State,10h	; PC/AT
	jz	short PC98
PCAT:
	in	AL,61h		;�r�[�vON
	or	AL,3
	out	61h,AL		; AT

	mov	DX,0012h 	; 1193.18K(AT�݊��@)
	mov	AX,34dch
	mov	BX,BEEP_CNT_AT
	jmp	short CLOCK8MHZ
	EVEN
PC98:
	;�r�[�vON	(��ɂ����Ă������Ǒ��v���Ȃ�)
	mov	AL,BEEP_ON
	out	BEEP_SW,AL	; 98

	mov	BX,BEEP_CNT

	;spval = (CLOCK_CHK ? 1997000L : 2458000L) / hertz;
	xor	DX,DX
	mov	ES,DX
	test	byte ptr ES:[0501H],80h
	mov	DX,001eh 	; 1996.8K(8MHz�n)
	mov	AX,7800h
	jnz	short CLOCK8MHZ
	mov	DX,0025h	; 2457.6K(10MHz�n)
	mov	AX,8000h
CLOCK8MHZ:
	cmp	CX,DX
	ja	short NORMAL
	mov	AX,0ffffh
	jmp	short PLAY
	EVEN
NORMAL:
	div	CX
PLAY:
	;�^�C�}�J�E���g�l�ݒ�
	mov	DX,BX	; BEEP_CNT
	out	DX,AL		; 98,AT
	mov	AL,AH
	out	DX,AL		; 98,AT

;	;�r�[�vON
;	mov	AL,BEEP_ON
;	out	BEEP_SW,AL

	xor	AX,AX			; NOTFIN
	ret
	EVEN

END_OF_SOUND:
	;esound[glb.scnt - 1].sptr = (uint far *)esound[glb.scnt - 1].sbuf;
	mov	AX,word ptr esound[BX].sbuf+2
	mov	word ptr esound[BX].sptr+2,AX
;	mov	AX,word ptr esound[BX].sbuf
;	mov	word ptr esound[BX].sptr,AX
	mov	word ptr esound[BX].sptr,0	; sbuf��offset��0�̂͂�
	mov	AX,FINISH
	ret
endfunc			; }
END
